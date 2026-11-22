#include "BOOT_ISOTP.h"

static CAN_HandleTypeDef *g_can;
static BlIsoTpMessageFn g_on_message;

/* 接收状态 */
static uint8_t rx_buf[BL_ISOTP_MAX];  //有效数据
static uint16_t rx_total;             //应收总字节
static uint16_t rx_used;              //已收有效字节
static uint8_t rx_active;             //是否正在接收一条多帧消息
static uint8_t rx_next_sn;            //下一帧应该是什么序号
static uint8_t rx_block_count;        //这一批已经收了几个连续帧
static uint32_t rx_deadline;          //最晚等到哪个时刻

/* 发送状态 */
static uint8_t tx_buf[BL_ISOTP_MAX];
static uint16_t tx_total;
static uint16_t tx_used;
static uint8_t tx_active;
static uint8_t tx_wait_fc;
static uint8_t tx_next_sn;
static uint8_t tx_bs;
static uint8_t tx_block_count;
static uint8_t tx_stmin_ms;
static uint32_t tx_next_time;
static uint32_t tx_deadline;

static uint8_t TimeReached(uint32_t now, uint32_t deadline)
{
    return ((int32_t)(now - deadline) >= 0) ? 1U : 0U;
}

static uint8_t CanSendFrame(const uint8_t data[8])
{
    CAN_TxHeaderTypeDef header = {0};
    uint32_t mailbox;

    if (g_can == NULL || HAL_CAN_GetTxMailboxesFreeLevel(g_can) == 0U)
    {
        return 0U;
    }

    header.StdId = BL_ISOTP_TX_ID;
    header.IDE = CAN_ID_STD;
    header.RTR = CAN_RTR_DATA;
    header.DLC = 8U;
    header.TransmitGlobalTime = DISABLE;

    return (HAL_CAN_AddTxMessage(g_can, &header, (uint8_t *)data, &mailbox) == HAL_OK) ? 1U : 0U;
}

void BlIsoTp_Init(CAN_HandleTypeDef *can, BlIsoTpMessageFn on_message)
{
    g_can = can;
    g_on_message = on_message;

    rx_active = 0U;
    tx_active = 0U;
    tx_wait_fc = 0U;
}

uint8_t BlIsoTp_Send(const uint8_t *data, uint16_t length)
{
    uint8_t frame[8] = {0};

    if (data == NULL || length == 0U || length > BL_ISOTP_MAX || g_can == NULL)
    {
        return 0U;
    }

    /* 这份实现同一时间只发送一条多帧消息。 */
    if (tx_active)
    {
        return 0U;
    }

    if (length <= 7U)
    {
        frame[0] = (uint8_t)length;
        memcpy(&frame[1], data, length);
        return CanSendFrame(frame);
    }

    /*
     * First Frame：
     * 前两字节表示总长度，后六字节是负载。
     */
    frame[0] = 0x10U | (uint8_t)((length >> 8) & 0x0FU);
    frame[1] = (uint8_t)length;
    memcpy(&frame[2], data, 6U);

    if (CanSendFrame(frame) == 0U)
    {
        return 0U;
    }

    memcpy(tx_buf, data, length);
    tx_total = length;
    tx_used = 6U;
    tx_next_sn = 1U;
    tx_block_count = 0U;
    tx_active = 1U;
    tx_wait_fc = 1U;
    tx_deadline = HAL_GetTick() + TP_TIMEOUT_MS;

    return 1U;
}

static void OnFlowControl(const uint8_t data[8])
{
    uint8_t flow_status = data[0] & 0x0FU;
    uint8_t stmin = data[2];

    if (!tx_active || !tx_wait_fc)
    {
        return;
    }

    if (flow_status == 1U)  /* Wait */
    {
        tx_deadline = HAL_GetTick() + TP_TIMEOUT_MS;
        return;
    }

    if (flow_status != 0U)  /* Overflow或无效状态 */
    {
        tx_active = 0U;
        tx_wait_fc = 0U;
        return;
    }

    if (stmin <= 0x7FU)
    {
        tx_stmin_ms = stmin;
    }
    else if (stmin >= 0xF1U && stmin <= 0xF9U)
    {
        tx_stmin_ms = 1U;
    }
    else
    {
        tx_active = 0U;
        tx_wait_fc = 0U;
        return;
    }

    tx_bs = data[1];
    tx_block_count = 0U;
    tx_wait_fc = 0U;
    tx_next_time = HAL_GetTick() + tx_stmin_ms;
    tx_deadline = HAL_GetTick() + TP_TIMEOUT_MS;
}

void BlIsoTp_OnFrame(const CAN_RxHeaderTypeDef *header, const uint8_t data[8])
{
    uint8_t type;

    if (header == NULL || data == NULL || header->IDE != CAN_ID_STD || header->RTR != CAN_RTR_DATA || header->StdId != BL_ISOTP_RX_ID || header->DLC != 8U)
    {
        return;
    }

    type = data[0] >> 4;

    if (type == 0U)  /* Single Frame */
    {
        uint8_t length = data[0] & 0x0FU;

        rx_active = 0U;  /* 新请求取代旧的不完整接收 */

        if (length >= 1U && length <= 7U && g_on_message != NULL)
        {
            g_on_message(&data[1], length);
        }
        return;
    }

    if (type == 1U)  /* First Frame */
    {
        uint16_t length = ((uint16_t)(data[0] & 0x0FU) << 8) | data[1];
        uint8_t fc[8] = {0x30U, RX_BLOCK_SIZE, RX_STMIN_MS};

        rx_active = 0U;

        if (length <= 7U || length > BL_ISOTP_MAX)
        {
            uint8_t overflow[8] = {0x32U};
            (void)CanSendFrame(overflow);
            return;
        }

        memcpy(rx_buf, &data[2], 6U);
        rx_total = length;
        rx_used = 6U;
        rx_next_sn = 1U;
        rx_block_count = 0U;
        rx_deadline = HAL_GetTick() + TP_TIMEOUT_MS;

        if (CanSendFrame(fc) == 1U)
        {
            rx_active = 1U;
        }
        return;
    }

    if (type == 2U)
    {
        uint16_t remaining;
        uint8_t count;

        if (!rx_active || (data[0] & 0x0FU) != rx_next_sn)
        {
            rx_active = 0U;
            return;
        }

        remaining = rx_total - rx_used;
        count = (remaining > 7U) ? 7U : (uint8_t)remaining;
        memcpy(&rx_buf[rx_used], &data[1], count);

        rx_used += count;
        rx_next_sn = (uint8_t)((rx_next_sn + 1U) & 0x0FU);
        rx_block_count++;
        rx_deadline = HAL_GetTick() + TP_TIMEOUT_MS;

        if (rx_used == rx_total)
        {
            rx_active = 0U;
            if (g_on_message != NULL)
            {
                g_on_message(rx_buf, rx_total);
            }
        }
        else if (rx_block_count >= RX_BLOCK_SIZE)
        {
            uint8_t fc[8] = {0x30U, RX_BLOCK_SIZE, RX_STMIN_MS};

            rx_block_count = 0U;
            if (CanSendFrame(fc) == 0U)
            {
                rx_active = 0U;
            }
        }
        return;
    }

    if (type == 3U)  /* Flow Control */
    {
        OnFlowControl(data);
    }
}

void BlIsoTp_Poll(void)
{
    uint32_t now = HAL_GetTick();

    if (rx_active && TimeReached(now, rx_deadline))
    {
        rx_active = 0U;
    }

    if (!tx_active)
    {
        return;
    }

    if (TimeReached(now, tx_deadline))
    {
        tx_active = 0U;
        tx_wait_fc = 0U;
        return;
    }

    if (tx_wait_fc || !TimeReached(now, tx_next_time))
    {
        return;
    }

    uint8_t frame[8] = {0};
    uint16_t remaining = tx_total - tx_used;
    uint8_t count =
        (remaining > 7U) ? 7U : (uint8_t)remaining;

    frame[0] = 0x20U | tx_next_sn;
    memcpy(&frame[1], &tx_buf[tx_used], count);

    if (CanSendFrame(frame) == 0U)
    {
        return;  /* 邮箱忙：下次Poll重试，序号不变 */
    }

    tx_used += count;
    tx_next_sn = (uint8_t)((tx_next_sn + 1U) & 0x0FU);
    tx_block_count++;
    tx_deadline = now + TP_TIMEOUT_MS;

    if (tx_used == tx_total)
    {
        tx_active = 0U;
    }
    else if (tx_bs != 0U && tx_block_count >= tx_bs)
    {
        tx_wait_fc = 1U;
        tx_deadline = now + TP_TIMEOUT_MS;
    }
    else
    {
        tx_next_time = now + tx_stmin_ms;
    }
}



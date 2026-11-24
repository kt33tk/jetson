#include "CanRX.h"

extern QueueHandle_t Queue_CAN_RX_t;
extern QueueHandle_t Queue_Control_Cmd_t;

extern volatile uint32_t can_rx_cnt;
extern volatile uint32_t can_busoff_cnt;
extern volatile uint32_t can_tx_fail_cnt;
extern volatile uint32_t control_cmd_queue_full_cnt;

void CanRX_Task(void *pvParameters)
{
    extern CAN_HandleTypeDef hcan1;

    Can_Frame_t raw_rx_msg;
    ControlCommand_t valid_cmd = {0};

    IsoTp_Init(Uds_HandleRequest);

    if (HAL_CAN_ActivateNotification(
            &hcan1,
            CAN_IT_TX_MAILBOX_EMPTY |
            CAN_IT_RX_FIFO0_MSG_PENDING |
            CAN_IT_ERROR_WARNING |
            CAN_IT_ERROR_PASSIVE |
            CAN_IT_BUSOFF |
            CAN_IT_LAST_ERROR_CODE |
            CAN_IT_ERROR) != HAL_OK)
    {
        Error_Handler();
    }

    for (;;)
    {
        if (xQueueReceive(Queue_CAN_RX_t,
                          &raw_rx_msg, portMAX_DELAY) != pdTRUE)
        {
            continue;
        }

        if (raw_rx_msg.id == ISOTP_REQUEST_ID)
        {
            IsoTp_OnCanFrame(&raw_rx_msg);
        }
        else if (raw_rx_msg.id == 0x200U)
        {
            memset(&valid_cmd, 0, sizeof(valid_cmd));

            Rx_Filter_Status_t status =
                Filter_And_Decode_0x200(&raw_rx_msg, &valid_cmd);

            if (status == RX_VALID_NEW)
            {
                valid_cmd.timestamp = xTaskGetTickCount();
                valid_cmd.valid = 1U;

                if (xQueueSend(Queue_Control_Cmd_t,
                               &valid_cmd, 0U) == pdPASS)
                {
                    Commit_0x200_Counter(valid_cmd.rx_counter);

                    Trigger_Send_0x280_ACK(
                        valid_cmd.rx_counter, ACK_SUCCESS
                    );
                }
                else
                {
                    control_cmd_queue_full_cnt++;
                }
            }
            else if (status == RX_DUPLICATE)
            {
                Trigger_Send_0x280_ACK(
                    raw_rx_msg.data[6] & 0x0FU, ACK_ERR_DUP
                );
            }
            else if (status == RX_ERR_CRC)
            {
                Trigger_Send_0x280_ACK(
                    raw_rx_msg.data[6] & 0x0FU, ACK_ERR_CRC
                );
            }
            else if (status == RX_ERR_PARAM)
            {
                Trigger_Send_0x280_ACK(
                    raw_rx_msg.data[6] & 0x0FU, ACK_ERR_PARAM
                );
            }
        }
    }
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CAN_RxHeaderTypeDef RxHeader;
    uint8_t RxData[8];
    Can_Frame_t raw_msg; // 使用你定义的 CAN 帧结构体
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    // 必须把数据读出来，否则会一直卡在中断里
    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &RxHeader, RxData) == HAL_OK) {
        can_rx_cnt++; // 统计 RX 次数

        // 1. 将硬件拿到的原始数据极速打包
        raw_msg.id = RxHeader.StdId;
        raw_msg.dlc = RxHeader.DLC;
        for(int i=0; i<8; i++) {
            raw_msg.data[i] = RxData[i];
        }

        // 2. 使用专用的 FromISR 函数将数据压入接收队列
        if (xQueueSendFromISR(Queue_CAN_RX_t, &raw_msg, &xHigherPriorityTaskWoken) != pdPASS)
        {
            can_rx_queue_full_cnt++;
        }

        // 3. 立即触发 FreeRTOS 的上下文切换（如果 CanRX_Task 优先级更高，会瞬间跳过去执行）
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *hcan)
{
    if (hcan->Instance != CAN1)
    {
        return;
    }

    uint32_t error_code = HAL_CAN_GetError(hcan);

    if ((error_code & (HAL_CAN_ERROR_TX_TERR0 |
                       HAL_CAN_ERROR_TX_ALST0)) != 0U)
    {
        CanMailbox_ReleaseFromISR();
    }

    if ((error_code & (HAL_CAN_ERROR_TX_TERR1 |
                       HAL_CAN_ERROR_TX_ALST1)) != 0U)
    {
        CanMailbox_ReleaseFromISR();
    }

    if ((error_code & (HAL_CAN_ERROR_TX_TERR2 |
                       HAL_CAN_ERROR_TX_ALST2)) != 0U)
    {
        CanMailbox_ReleaseFromISR();
    }

    uint32_t esr = hcan->Instance->ESR;

    can_last_error = error_code;

    can_last_tec =
        (uint8_t)((esr & CAN_ESR_TEC_Msk) >> CAN_ESR_TEC_Pos);

    can_last_rec =
        (uint8_t)((esr & CAN_ESR_REC_Msk) >> CAN_ESR_REC_Pos);

    /*
     * 同一次回调中可能同时存在ACK和TX_TERR，
     * 所以整个mask只累加一次，避免一帧重复计数。
     */
    if (error_code &
        (HAL_CAN_ERROR_ACK |
         HAL_CAN_ERROR_BR |
         HAL_CAN_ERROR_BD |
         HAL_CAN_ERROR_STF |
         HAL_CAN_ERROR_FOR |
         HAL_CAN_ERROR_CRC |
         HAL_CAN_ERROR_TX_TERR0 |
         HAL_CAN_ERROR_TX_TERR1 |
         HAL_CAN_ERROR_TX_TERR2 |
         HAL_CAN_ERROR_TX_ALST0 |
         HAL_CAN_ERROR_TX_ALST1 |
         HAL_CAN_ERROR_TX_ALST2))
    {
        can_tx_fail_cnt++;
    }

    if (error_code & HAL_CAN_ERROR_ACK)
    {
        can_ack_error_cnt++;
    }

    if (error_code &
        (HAL_CAN_ERROR_BR |
         HAL_CAN_ERROR_BD |
         HAL_CAN_ERROR_STF |
         HAL_CAN_ERROR_FOR |
         HAL_CAN_ERROR_CRC))
    {
        can_bit_error_cnt++;
    }

    if (error_code & HAL_CAN_ERROR_BOF)
    {
        can_busoff_cnt++;
    }

    /*
     * HAL的ErrorCode是累积OR保存的。
     * 不清除的话，下一次回调可能还包含旧错误。
     */
    HAL_CAN_ResetError(hcan);
}

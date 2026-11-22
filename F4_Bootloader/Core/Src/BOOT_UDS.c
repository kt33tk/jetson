#include "BOOT_UDS.h"

static struct
{
	uint8_t session;          //是否进入编程会话
	uint8_t active;           //是否正在接收固件数据块
	uint8_t exited;           //数据是否收齐，并且已经收到 37 正式结束传输
	uint8_t next_block;       //下一条 36 请求应该带的块序号
	uint32_t size;            //固件字节数
	uint32_t received;        //目前已经成功写入并读回确认了多少字节
}download;

static uint8_t flash_ready;   //外部传给本模块的片外 Flash 就绪标志
static uint8_t job_sid;       //哪条 UDS 请求正在等待长任务结果
static uint8_t reset_pending; //已安排复位但未执行

static uint32_t last_request;
static uint32_t pending_tick;
static uint32_t reset_tick;

static uint32_t ReadBe32(const uint8_t *data)
{
    return ((uint32_t)data[0] << 24) |
           ((uint32_t)data[1] << 16) |
           ((uint32_t)data[2] << 8) |
           data[3];
}

static uint8_t Nrc(uint8_t sid, uint8_t code)
{
    uint8_t response[3] = {0x7FU, sid, code};
    return BlIsoTp_Send(response, sizeof(response));
}

static void ScheduleReset(void)
{
    reset_pending = 1U;
    reset_tick = HAL_GetTick();
}

void BootUds_Init(uint8_t ready)
{
    memset(&download, 0, sizeof(download));

    flash_ready = ready;
    job_sid = 0U;
    reset_pending = 0U;
    last_request = HAL_GetTick();
}

static void HandleSession(const uint8_t *request, uint16_t length)
{
    if (length != 2U)
    {
        Nrc(0x10U, 0x13U);
        return;
    }

    if (request[1] != 0x02U)
    {
        Nrc(0x10U, 0x12U);
        return;
    }

    memset(&download, 0, sizeof(download));
    download.session = 1U;

    uint8_t response[2] = {0x50U, 0x02U};
    BlIsoTp_Send(response, sizeof(response));
}

static void HandleDownload(const uint8_t *request, uint16_t length)
{
    if (length != 11U)
    {
        Nrc(0x34U, 0x13U);
        return;
    }

    if (download.active)
    {
        Nrc(0x34U, 0x24U);
        return;
    }

    uint32_t address = ReadBe32(&request[3]);
    uint32_t size = ReadBe32(&request[7]);

    if (request[1] != 0U || request[2] != 0x44U ||
        address != APP_BASE ||
        size < 8U || size > APP_END - APP_BASE)
    {
        Nrc(0x34U, 0x31U);
        return;
    }

    if (!flash_ready || !Image_StartDownload(size))
    {
        Nrc(0x34U, 0x22U);
        return;
    }

    download.size = size;
    download.received = 0U;
    download.active = 0U;
    download.exited = 0U;
    download.next_block = 1U;

    job_sid = 0x34U;
    pending_tick = HAL_GetTick() - 500U;
}

static void HandleTransfer(const uint8_t *request, uint16_t length)
{
    if (length < 3U)
    {
        Nrc(0x36U, 0x13U);
        return;
    }

    if (!download.active)
    {
        Nrc(0x36U, 0x24U);
        return;
    }

    if (request[1] != download.next_block)
    {
        Nrc(0x36U, 0x73U);
        return;
    }

    uint16_t count = length - 2U;

    if (count > download.size - download.received)
    {
        Nrc(0x36U, 0x31U);
        return;
    }

    if (!Image_Write(download.received, &request[2], count))
    {
        download.active = 0U;
        Nrc(0x36U, 0x72U);
        ScheduleReset();
        return;
    }

    download.received += count;
    download.next_block++;

    uint8_t response[2] = {0x76U, request[1]};
    BlIsoTp_Send(response, sizeof(response));
}

static void HandleExit(uint16_t length)
{
    if (length != 1U)
    {
        Nrc(0x37U, 0x13U);
        return;
    }

    if (!download.active || download.received != download.size)
    {
        Nrc(0x37U, 0x24U);
        return;
    }

    download.active = 0U;
    download.exited = 1U;

    uint8_t response = 0x77U;
    BlIsoTp_Send(&response, 1U);
}

static void HandleVerify(const uint8_t *request, uint16_t length)
{
    if (length != 8U)
    {
        Nrc(0x31U, 0x13U);
        return;
    }

    if (request[1] != 0x01U ||
        request[2] != 0xFFU ||
        request[3] != 0x00U)
    {
        Nrc(0x31U, 0x31U);
        return;
    }

    if (!download.exited)
    {
        Nrc(0x31U, 0x24U);
        return;
    }

    if (!Image_StartInstall(download.size, ReadBe32(&request[4])))
    {
        Nrc(0x31U, 0x22U);
        ScheduleReset();
        return;
    }

    job_sid = 0x31U;
    pending_tick = HAL_GetTick() - 500U;
}

static void HandleReset(const uint8_t *request, uint16_t length)
{
    if (length != 2U)
    {
        Nrc(0x11U, 0x13U);
        return;
    }

    if (request[1] != 0x01U)
    {
        Nrc(0x11U, 0x12U);
        return;
    }

    uint8_t response[2] = {0x51U, 0x01U};
    BlIsoTp_Send(response, sizeof(response));
    ScheduleReset();
}

/*
 * 22 F1 80           读取内部 AppInfo 中真实的 App 版本
 * 22 F1 A0           读取 ECU 硬件 ID
 * 22 F1 A1           读取 Bootloader 版本
 * 22 F1 A2           返回 01，表示当前在 Bootloader
 * 22 F1 A3           返回 Metadata 状态、镜像处理阶段、App 有效性
 */

static void HandleReadInfo(const uint8_t *request, uint16_t length)
{
    if (length != 3U)
    {
        Nrc(0x22U, 0x13U);
        return;
    }

    uint16_t did = ((uint16_t)request[1] << 8) | request[2];

    uint8_t response[7] = {0x62U, request[1], request[2], 0U, 0U, 0U, 0U};

    switch (did)
    {
    case 0xF180U:
    {
        const AppInfo_t *info = (const AppInfo_t *)APP_INFO_ADDRESS;

        if (!Image_AppValid() || info->magic != APP_INFO_MAGIC || info->format != APP_INFO_FORMAT)
        {
            Nrc(0x22U, 0x31U);
            return;
        }

        response[3] = info->major;
        response[4] = info->minor;
        response[5] = info->patch;

        BlIsoTp_Send(response, 6U);
        return;
    }

    case 0xF1A0U:
        response[3] = (uint8_t)(ECU_HARDWARE_ID >> 24);
        response[4] = (uint8_t)(ECU_HARDWARE_ID >> 16);
        response[5] = (uint8_t)(ECU_HARDWARE_ID >> 8);
        response[6] = (uint8_t)ECU_HARDWARE_ID;

        BlIsoTp_Send(response, 7U);
        return;

    case 0xF1A1U:
        response[3] = BOOT_SW_MAJOR;
        response[4] = BOOT_SW_MINOR;
        response[5] = BOOT_SW_PATCH;

        BlIsoTp_Send(response, 6U);
        return;

    case 0xF1A2U:
        response[3] = 1U;

        BlIsoTp_Send(response, 4U);
        return;

    case 0xF1A3U:
    {
        const BootMeta_t *meta = (const BootMeta_t *)META_BASE;

        response[3] = meta->magic == META_MAGIC ? (uint8_t)meta->state : 0xFFU;

        response[4] = (uint8_t)image_stage;
        response[5] = Image_AppValid();

        BlIsoTp_Send(response, 6U);
        return;
    }

    default:
        Nrc(0x22U, 0x31U);
        return;
    }
}

static void HandleTesterPresent(const uint8_t *request, uint16_t length)
{
    if (length != 2U)
    {
        Nrc(0x3EU, 0x13U);
        return;
    }

    if (request[1] != 0x00U)
    {
        Nrc(0x3EU, 0x12U);
        return;
    }

    last_request = HAL_GetTick();

    uint8_t response[2] = {0x7EU, 0x00U};
    BlIsoTp_Send(response, sizeof(response));
}

void BootUds_OnMessage(const uint8_t *request, uint16_t length)
{
    if (length == 0U || reset_pending)
        return;

    uint8_t sid = request[0];
    last_request = HAL_GetTick();

    if (job_sid != 0U)
    {
        Nrc(sid, 0x21U);
        return;
    }

    switch (sid)
    {
    case 0x10U:
        HandleSession(request, length);
        return;

    case 0x22U:
        HandleReadInfo(request, length);
        return;

    case 0x3EU:
        HandleTesterPresent(request, length);
        return;
    }

    if (sid != 0x34U && sid != 0x36U &&
        sid != 0x37U && sid != 0x31U && sid != 0x11U)
    {
        Nrc(sid, 0x11U);
        return;
    }

    if (!download.session)
    {
        Nrc(sid, 0x7FU);
        return;
    }

    switch (sid)
    {
    case 0x34U:
        HandleDownload(request, length);
        break;

    case 0x36U:
        HandleTransfer(request, length);
        break;

    case 0x37U:
        HandleExit(length);
        break;

    case 0x31U:
        HandleVerify(request, length);
        break;

    case 0x11U:
        HandleReset(request, length);
        break;
    }
}

void BootUds_Poll(CAN_HandleTypeDef *can)
{
    uint32_t now = HAL_GetTick();

    if (reset_pending)
    {
        uint32_t elapsed = now - reset_tick;

        if (elapsed >= 100U && (HAL_CAN_GetTxMailboxesFreeLevel(can) == 3U || elapsed >= 500U))
        {
            NVIC_SystemReset();
        }

        return;
    }

    if (job_sid != 0U)
    {
        if (now - pending_tick >= 500U)
        {
            if (Nrc(job_sid, 0x78U))
                pending_tick = now;
        }

        int result = Image_Poll();

        if (result == 0)
            return;

        if (result < 0)
        {
            Nrc(job_sid, 0x72U);
            job_sid = 0U;
            ScheduleReset();
            return;
        }

        uint8_t response[4];

        if (job_sid == 0x34U)
        {
            response[0] = 0x74U;
            response[1] = 0x20U;
            response[2] = 0x01U;
            response[3] = 0x00U;
        }
        else
        {
            response[0] = 0x71U;
            response[1] = 0x01U;
            response[2] = 0xFFU;
            response[3] = 0x00U;
        }

        /*
         * Image_Poll完成后会保持DONE状态。
         * 邮箱暂时忙，下次Poll继续尝试正式响应。
         */
        if (!BlIsoTp_Send(response, sizeof(response)))
            return;

        if (job_sid == 0x34U)
            download.active = 1U;

        job_sid = 0U;
        last_request = HAL_GetTick();
        return;
    }

    if (download.session && now - last_request >= 10000U)
        ScheduleReset();
}

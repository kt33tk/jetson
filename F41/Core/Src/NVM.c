#include "NVM.h"

const NvM_ParamPayload_t NvM_DefaultParams =
{
    .auto_on_temp_x10 = 300,
    .auto_off_temp_x10 = 280,
    .manual_timeout_ms = 2000U,
    .sensor_timeout_ms = 2000U,
    .reserved = {0}
};

extern QueueHandle_t Queue_NVM_t;
extern QueueHandle_t Queue_Control_Config_t;

static uint32_t g_active_param_base = NVM_PARAM_A_BASE;
static uint32_t g_active_param_sequence = 0U;
static bool g_active_param_valid = false;

static uint32_t g_dtc_next_sequence = 1U;
static uint32_t g_dtc_write_index = 0U;

volatile uint32_t nvm_queue_full_count = 0U;

volatile uint32_t nvm_param_save_ok_count = 0U;
volatile uint32_t nvm_param_save_fail_count = 0U;
volatile uint32_t nvm_dtc_write_ok_count = 0U;
volatile uint32_t nvm_dtc_write_fail_count = 0U;

static bool g_nvm_available = false;

//检查参数值是否合理
static bool NvM_ParamPayloadValid(const NvM_ParamPayload_t *params)
{
    if (params == NULL)
    {
        return false;
    }

    if (params->auto_off_temp_x10 >= params->auto_on_temp_x10)
    {
        return false;
    }

    if ((params->auto_on_temp_x10 < -400) || (params->auto_on_temp_x10 > 1250))
    {
        return false;
    }

    if ((params->auto_off_temp_x10 < -400) || (params->auto_off_temp_x10 > 1250))
    {
        return false;
    }

    if ((params->manual_timeout_ms < 100U) || (params->manual_timeout_ms > 60000U))
    {
        return false;
    }

    if ((params->sensor_timeout_ms < 100U) || (params->sensor_timeout_ms > 60000U))
    {
        return false;
    }

    return true;
}

//计算CRC
static uint32_t NvM_ParamCalculateCrc(const NvM_ParamRecord_t *record)
{
    return CRC32_Calculate(record, offsetof(NvM_ParamRecord_t, crc32));
}

//检查magic,format等
static bool NvM_ParamBodyValid(const NvM_ParamRecord_t *record)
{
    if (record->magic != NVM_PARAM_MAGIC)
    {
        return false;
    }

    if (record->format_version != NVM_PARAM_VERSION)
    {
        return false;
    }

    if (record->payload_length != sizeof(NvM_ParamPayload_t))
    {
        return false;
    }

    if (!NvM_ParamPayloadValid(&record->payload))
    {
        return false;
    }

    if (record->crc32 != NvM_ParamCalculateCrc(record))
    {
        return false;
    }

    return true;
}

//检查最后提交标记(防断电)
static bool NvM_ParamRecordValid(const NvM_ParamRecord_t *record)
{
    if (!NvM_ParamBodyValid(record))
    {
        return false;
    }

    return record->commit_marker == NVM_PARAM_COMMIT_MARKER;
}

//从A/B读出数据并且判断是否完整有效
static bool NvM_ReadParamSlot(uint32_t address, NvM_ParamRecord_t *record)
{
    if (record == NULL)
    {
        return false;
    }

    if (W25Q64_Read(address, (uint8_t *)record, sizeof(*record)) != W25Q64_OK)
    {
        return false;
    }

    return NvM_ParamRecordValid(record);
}

//比较序号新旧（考虑溢出）
static bool NvM_SequenceNewer(uint32_t a, uint32_t b)
{
    return ((int32_t)(a - b)) > 0;
}

//同时读取AB，并选择最新（无效时使用默认参数）
static bool NvM_LoadParameters(NvM_ParamPayload_t *output)
{
    NvM_ParamRecord_t slot_a;
    NvM_ParamRecord_t slot_b;

    bool valid_a = NvM_ReadParamSlot(NVM_PARAM_A_BASE, &slot_a);

    bool valid_b = NvM_ReadParamSlot(NVM_PARAM_B_BASE, &slot_b);

    const NvM_ParamRecord_t *selected = NULL;
    uint32_t selected_base = 0U;

    if (valid_a && valid_b)
    {
        if (NvM_SequenceNewer(slot_b.sequence, slot_a.sequence))
        {
            selected = &slot_b;
            selected_base = NVM_PARAM_B_BASE;
        }
        else
        {
            selected = &slot_a;
            selected_base = NVM_PARAM_A_BASE;
        }
    }
    else if (valid_a)
    {
        selected = &slot_a;
        selected_base = NVM_PARAM_A_BASE;
    }
    else if (valid_b)
    {
        selected = &slot_b;
        selected_base = NVM_PARAM_B_BASE;
    }

    if (selected == NULL)
    {
        *output = NvM_DefaultParams;

        g_active_param_valid = false;
        g_active_param_sequence = 0U;
        g_active_param_base = NVM_PARAM_A_BASE;

        return false;
    }

    *output = selected->payload;

    g_active_param_valid = true;
    g_active_param_sequence = selected->sequence;
    g_active_param_base = selected_base;

    return true;
}

//业务包装为完整Flash记录（不写market防断电）
static void NvM_BuildParamRecord(NvM_ParamRecord_t *record, const NvM_ParamPayload_t *params, uint32_t sequence)
{
    memset(record, 0xFF, sizeof(*record));

    record->magic = NVM_PARAM_MAGIC;
    record->format_version = NVM_PARAM_VERSION;
    record->payload_length = sizeof(NvM_ParamPayload_t);

    record->sequence = sequence;
    record->payload = *params;

    record->crc32 = NvM_ParamCalculateCrc(record);

    /*
     * 此时不能先设置提交标记。
     * 必须等主体写入并回读成功后再写。
     */
    record->commit_marker = 0xFFFFFFFFUL;
}

//完整AB安全保存过程
static bool NvM_SaveParametersNow(const NvM_ParamPayload_t *params)
{
    NvM_ParamRecord_t record;
    NvM_ParamRecord_t verify;

    uint32_t target_base;
    uint32_t next_sequence;

    if (!NvM_ParamPayloadValid(params))
    {
        return false;
    }

    if (!g_active_param_valid)
    {
        target_base = NVM_PARAM_A_BASE;
        next_sequence = 1U;
    }
    else
    {
        target_base = (g_active_param_base == NVM_PARAM_A_BASE) ? NVM_PARAM_B_BASE : NVM_PARAM_A_BASE;

        next_sequence = g_active_param_sequence + 1U;
    }

    NvM_BuildParamRecord(&record, params, next_sequence);

    if (W25Q64_EraseSector4K(target_base) != W25Q64_OK)
    {
        return false;
    }

    /*
     * 只写到 commit_marker 之前。
     * 此时槽仍然不是已提交状态。
     */
    if (W25Q64_Write(target_base, (const uint8_t *)&record, offsetof(NvM_ParamRecord_t, commit_marker)) != W25Q64_OK)
    {
        return false;
    }

    if (W25Q64_Read(target_base, (uint8_t *)&verify, sizeof(verify)) != W25Q64_OK)
    {
        return false;
    }

    if (!NvM_ParamBodyValid(&verify))
    {
        return false;
    }

    uint32_t commit = NVM_PARAM_COMMIT_MARKER;

    if (W25Q64_Write(target_base + offsetof(NvM_ParamRecord_t, commit_marker), (const uint8_t *)&commit, sizeof(commit)) != W25Q64_OK)
    {
        return false;
    }

    if (!NvM_ReadParamSlot(target_base, &verify))
    {
        return false;
    }

    /*
     * 只有全部验证成功后才切换活动槽。
     */
    g_active_param_base = target_base;
    g_active_param_sequence = next_sequence;
    g_active_param_valid = true;

    return true;
}

//DTC

//检查擦除结果
static bool NvM_BufferIsErased(const uint8_t *data, size_t length)
{
    for (size_t i = 0; i < length; i++)
    {
        if (data[i] != 0xFFU)
        {
            return false;
        }
    }

    return true;
}

//检查magic，版本等
static bool NvM_DtcValid(const NvM_DtcRecord_t *record)
{
    if (record->magic != NVM_DTC_MAGIC)
    {
        return false;
    }

    if (record->version != NVM_DTC_VERSION)
    {
        return false;
    }

    uint32_t calculated = CRC32_Calculate(record, offsetof(NvM_DtcRecord_t, crc32));

    return calculated == record->crc32;
}

//记录编号换算为Flash地址
static uint32_t NvM_DtcAddress(uint32_t index)
{
    return NVM_DTC_BASE + index * sizeof(NvM_DtcRecord_t);
}

//根据编号寻找所在扇区起始地址
static uint32_t NvM_DtcSectorBase(uint32_t index)
{
    uint32_t sector_index = index / NVM_DTC_RECORDS_PER_SECTOR;

    return NVM_DTC_BASE + sector_index * W25Q64_SECTOR_SIZE;
}

//检查编号合法性
static bool NvM_DtcIndexValid(uint32_t index)
{
    return index < NVM_DTC_TOTAL_RECORDS;
}

//扫描找到最新DTC以及下一次写入位置
static void NvM_DtcScan(void)
{
    NvM_DtcRecord_t record;

    bool found_valid = false;
    uint32_t newest_sequence = 0U;
    uint32_t newest_index = 0U;

    for (uint32_t index = 0U; index < NVM_DTC_TOTAL_RECORDS; index++)
    {
        if (W25Q64_Read(NvM_DtcAddress(index), (uint8_t *)&record, sizeof(record)) != W25Q64_OK)
        {
            continue;
        }

        if (NvM_DtcValid(&record))
        {
            if ((!found_valid) || NvM_SequenceNewer(record.sequence, newest_sequence))
            {
                found_valid = true;
                newest_sequence = record.sequence;
                newest_index = index;
            }
        }
    }

    if (!found_valid)
    {
        g_dtc_next_sequence = 1U;
        g_dtc_write_index = 0U;
        return;
    }

    g_dtc_next_sequence = newest_sequence + 1U;

    g_dtc_write_index = (newest_index + 1U) % NVM_DTC_TOTAL_RECORDS;
}

//判断目标槽状态
static NvM_DtcSlotState_t NvM_DtcGetSlotState(uint32_t index, NvM_DtcRecord_t *record)
{
    if ((!NvM_DtcIndexValid(index)) || (record == NULL))
    {
        return NVM_DTC_SLOT_READ_ERROR;
    }

    if (W25Q64_Read(NvM_DtcAddress(index), (uint8_t *)record, sizeof(*record)) != W25Q64_OK)
    {
        return NVM_DTC_SLOT_READ_ERROR;
    }

    if (NvM_BufferIsErased((const uint8_t *)record, sizeof(*record)))
    {
        return NVM_DTC_SLOT_ERASED;
    }

    return NVM_DTC_SLOT_OCCUPIED;
}

//控制任务提交的数据包装为Flash并计算CRC
static bool NvM_BuildDtcRecord(NvM_DtcRecord_t *record, const NvM_DtcRequestData_t *request)
{
    if ((record == NULL) || (request == NULL))
    {
        return false;
    }

    if ((request->event != NVM_DTC_EVENT_SET) && (request->event != NVM_DTC_EVENT_CLEAR))
    {
        return false;
    }

    memset(record, 0, sizeof(*record));

    record->magic = NVM_DTC_MAGIC;
    record->version = NVM_DTC_VERSION;
    record->event = request->event;

    record->sequence = g_dtc_next_sequence;
    record->timestamp = request->timestamp;

    record->dtc_code = request->dtc_code;
    record->dtc_status = request->dtc_status;
    record->control_mode = request->control_mode;

    record->temperature_x10 = request->temperature_x10;

    record->app_value = request->app_value;
    record->relay_state = request->relay_state;
    record->can_error = request->can_error;
    record->reserved = 0U;

    record->crc32 = CRC32_Calculate(record, offsetof(NvM_DtcRecord_t, crc32));

    return true;
}

//写入前寻找空槽，进入旧扇区先擦除整个旧扇区
static bool NvM_DtcPrepareWriteSlot(void)
{
    NvM_DtcRecord_t record;

    for (uint32_t attempt = 0U; attempt < NVM_DTC_TOTAL_RECORDS; attempt++)
    {
        NvM_DtcSlotState_t state = NvM_DtcGetSlotState(g_dtc_write_index, &record);

        if (state == NVM_DTC_SLOT_READ_ERROR)
        {
            return false;
        }

        if (state == NVM_DTC_SLOT_ERASED)
        {
            return true;
        }

        /*
         * 如果目标位于一个扇区的第一个记录，
         * 说明环形指针已经进入旧扇区，可以擦除它。
         */
        if ((g_dtc_write_index % NVM_DTC_RECORDS_PER_SECTOR) == 0U)
        {
            uint32_t sector_base =NvM_DtcSectorBase(  g_dtc_write_index);

            if (W25Q64_EraseSector4K(sector_base) != W25Q64_OK)
            {
                return false;
            }

            /*
             * 擦除后必须再检查，不能假定擦除一定成功。
             */
            state = NvM_DtcGetSlotState(g_dtc_write_index, &record);

            return state == NVM_DTC_SLOT_ERASED;
        }

        /*
         * 当前不是扇区首地址，说明可能遇到了
         * 掉电造成的半写记录，直接跳过。
         */
        g_dtc_write_index++;

        if (g_dtc_write_index >= NVM_DTC_TOTAL_RECORDS)
        {
            g_dtc_write_index = 0U;
        }
    }

    return false;
}

//写入DTC，成功后推进序号和写指针
static bool NvM_AppendDtcNow(const NvM_DtcRequestData_t *request)
{
    NvM_DtcRecord_t record;
    NvM_DtcRecord_t verify;

    if (request == NULL)
    {
        return false;
    }

    if (!NvM_DtcPrepareWriteSlot())
    {
        return false;
    }

    if (!NvM_BuildDtcRecord(&record, request))
    {
        return false;
    }

    uint32_t address = NvM_DtcAddress(g_dtc_write_index);

    if (W25Q64_Write(address, (const uint8_t *)&record, sizeof(record)) != W25Q64_OK)
    {
        /*
         * 写失败时暂不推进序号。
         * 下一次调用会检查该槽是否已被部分写入。
         */
        return false;
    }

    if (W25Q64_Read(address, (uint8_t *)&verify, sizeof(verify)) != W25Q64_OK)
    {
        return false;
    }

    if (!NvM_DtcValid(&verify))
    {
        return false;
    }

    if (verify.sequence != record.sequence)
    {
        return false;
    }

    g_dtc_next_sequence++;
    g_dtc_write_index++;

    if (g_dtc_write_index >= NVM_DTC_TOTAL_RECORDS)
    {
        g_dtc_write_index = 0U;
    }

    return true;
}

//DTC请求复制到NVM队列
uint8_t NvM_RequestDtc(const NvM_DtcRequestData_t *dtc)
{
    NvM_Request_t request;

    if ((Queue_NVM_t == NULL) || (dtc == NULL))
    {
        return 0U;
    }

    memset(&request, 0, sizeof(request));

    request.type = NVM_REQUEST_APPEND_DTC;
    request.data.dtc = *dtc;

    if (xQueueSend(Queue_NVM_t, &request, 0U) != pdPASS)
    {
        nvm_queue_full_count++;
        return 0U;
    }

    return 1U;
}

//参数保存请求复制到NVM队列
uint8_t NvM_RequestSaveParams(const NvM_ParamPayload_t *params)
{
    NvM_Request_t request;

    if ((Queue_NVM_t == NULL) || (params == NULL) || (!NvM_ParamPayloadValid(params)))
    {
        return 0U;
    }

    request.type = NVM_REQUEST_SAVE_PARAMS;
    request.data.params = *params;
    request.reply_task = NULL;

    if (xQueueSend(Queue_NVM_t, &request, 0U) != pdPASS)
    {
        nvm_queue_full_count++;
        return 0U;
    }

    return 1U;
}

uint8_t NvM_ReadLatestSetDtc(uint16_t dtc_code, NvM_DtcRecord_t *output)
{
    NvM_DtcRecord_t record;
    uint32_t start_index;

    if ((!g_nvm_available) || (output == NULL))
    {
        return 0U;
    }

    start_index = g_dtc_write_index;

    for (uint32_t offset = 1U; offset <= NVM_DTC_TOTAL_RECORDS; offset++)
    {
        uint32_t index = (start_index + NVM_DTC_TOTAL_RECORDS - offset) % NVM_DTC_TOTAL_RECORDS;

        if (W25Q64_Read(NvM_DtcAddress(index), (uint8_t *)&record, sizeof(record)) != W25Q64_OK)
        {
            continue;
        }

        if (NvM_DtcValid(&record) && (record.event == NVM_DTC_EVENT_SET) && (record.dtc_code == dtc_code))
        {
            *output = record;
            return 1U;
        }
    }

    return 0U;
}

static bool NvM_ClearDtcNow(void)
{
    for (uint32_t sector = 0U; sector < NVM_DTC_SECTOR_COUNT; sector++)
    {
        uint32_t address = NVM_DTC_BASE + sector * W25Q64_SECTOR_SIZE;

        if (W25Q64_EraseSector4K(address) != W25Q64_OK)
        {
            return false;
        }
    }

    g_dtc_next_sequence = 1U;
    g_dtc_write_index = 0U;

    return true;
}

uint8_t NvM_ClearDtcHistory(void)
{
    NvM_Request_t request;
    uint32_t result = 0U;

    if (Queue_NVM_t == NULL)
    {
        return 0U;
    }

    memset(&request, 0, sizeof(request));

    request.type = NVM_REQUEST_CLEAR_DTC;
    request.reply_task = xTaskGetCurrentTaskHandle();

    if (xQueueSend(Queue_NVM_t, &request, 0U) != pdPASS)
    {
        nvm_queue_full_count++;
        return 0U;
    }

    if (xTaskNotifyWait(0U, UINT32_MAX, &result, pdMS_TO_TICKS(5000)) != pdTRUE)
    {
        return 0U;
    }

    return (result == 1U) ? 1U : 0U;
}

//写入Flash
void NvM_Task(void *pvParameters)
{
    NvM_Request_t request;

    (void)pvParameters;

    for (;;)
    {
        if (xQueueReceive(Queue_NVM_t, &request, portMAX_DELAY) != pdPASS)
        {
            continue;
        }

        if (!g_nvm_available)
        {
            /*
             * 后续可以在这里转交给 RAM DTC。
             * 当前先统计失败并丢弃请求。
             */
            if (request.type == NVM_REQUEST_APPEND_DTC)
            {
                nvm_dtc_write_fail_count++;
            }
            else if (request.type == NVM_REQUEST_SAVE_PARAMS)
            {
                nvm_param_save_fail_count++;
            }

            if ((request.type == NVM_REQUEST_CLEAR_DTC) && (request.reply_task != NULL))
            {
                xTaskNotify(request.reply_task, 2U, eSetValueWithOverwrite);
            }

            continue;
        }

        switch (request.type)
        {
        case NVM_REQUEST_SAVE_PARAMS:
            if (NvM_SaveParametersNow( &request.data.params))
            {
                nvm_param_save_ok_count++;

                if (Queue_Control_Config_t != NULL)
                {
                    xQueueOverwrite(Queue_Control_Config_t, &request.data.params);
                }
            }
            else
            {
                nvm_param_save_fail_count++;
            }
            break;

        case NVM_REQUEST_APPEND_DTC:
            if (NvM_AppendDtcNow( &request.data.dtc))
            {
                nvm_dtc_write_ok_count++;
            }
            else
            {
                nvm_dtc_write_fail_count++;
            }
            break;

        case NVM_REQUEST_CLEAR_DTC:
        {
            uint32_t result = NvM_ClearDtcNow() ? 1U : 2U;

            if (request.reply_task != NULL)
            {
                xTaskNotify(request.reply_task,
                            result,
                            eSetValueWithOverwrite);
            }

            break;
        }

        default:
            break;
        }
    }
}

//启动时自检CRC，初始化W25Q64，加载参数，扫描DTC
uint8_t NvM_BootInit(NvM_ParamPayload_t *loaded_params)
{
    if (loaded_params == NULL)
    {
        return 0U;
    }

    *loaded_params = NvM_DefaultParams;
    g_nvm_available = false;

    if (SystemEventGroup != NULL)
    {
        xEventGroupClearBits(SystemEventGroup, EVT_NVM_READY);
    }

    if (CRC32_SelfTest() == 0U)
    {
        return 0U;
    }

    if (W25Q64_Init() != W25Q64_OK)
    {
        /*
         * Flash 不可用时继续使用默认参数。
         * 控制、CAN 和 Watchdog 不应停止。
         */
        return 0U;
    }

    /*
     * false 只表示 A/B 都无效并使用默认值，
     * 不表示 Flash 硬件不可用。
     */
    (void)NvM_LoadParameters(loaded_params);

    NvM_DtcScan();

    g_nvm_available = true;

    if (SystemEventGroup != NULL)
    {
        xEventGroupSetBits(SystemEventGroup, EVT_NVM_READY);
    }

    return 1U;
}


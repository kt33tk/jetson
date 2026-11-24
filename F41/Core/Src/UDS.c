#include "UDS.h"

//发送否定响应
static void Uds_SendNrc(uint8_t sid, uint8_t nrc)
{
    uint8_t response[3] = {0x7FU, sid, nrc};
    IsoTp_Send(response, sizeof(response));
}

//32位整数->4字节
static void PutU32(uint8_t *output, uint32_t value)
{
    output[0] = (uint8_t)(value >> 24U);
    output[1] = (uint8_t)(value >> 16U);
    output[2] = (uint8_t)(value >> 8U);
    output[3] = (uint8_t)value;
}

//故障码->DTC编码
static uint16_t Uds_DtcCodeFromBit(uint8_t bit)
{
    if (bit < 4U)
    {
        return (uint16_t)(0x1001U + bit);
    }

    return (uint16_t)(0x1080U + bit);
}

/*
*          服务	                               request[] 请求格式	                      response[] 肯定响应格式
*      0x10 进入默认会话	                               10 01	                                 50 01
*      0x10 OTA 编程会话                              10 02                            App：7F 10 78，Bootloader：50 02
*      0x3E 在线检测	                               3E 00	                                 7E 00
*      0x22 F180 软件版本	                          22 F1 80	                        62 F1 80 主版本 次版本 补丁版本
*      0x22 F181 传感器数据	                          22 F1 81	                        62 F1 81 温度高 温度低 有效标志
*      0x22 F182 控制状态	                          22 F1 82	                        62 F1 82 模式 故障位 应用值 继电器
*      0x22 F183 CAN 统计	                          22 F1 83	                        62 F1 83 + 四个 32 位统计数
*      0x22 F1A0 硬件 ID                           22 F1 A0                             62 F1 A0 04 07 00 01
*      0x22 F1A2 当前程序模式                         22 F1 A2                          62 F1 A2 00（表示当前为 App）
*      0x19 02 故障列表	                         19 02 状态掩码	                    59 02 支持掩码 + 若干条 DTC
*      0x19 04 故障快照	                  19 04 DTC高 DTC中 DTC低 快照号                	59 04 + DTC 信息 + 历史快照
*      0x14 清除历史	                            14 FF FF FF	                                   54
*/

static void Uds_HandleNormalRequest(const uint8_t *request, uint16_t length)
{
    uint8_t response[ISOTP_MAX_PAYLOAD] = {0};
    DiagSnapshot_t snapshot;

    if ((request == NULL) || (length == 0U))
    {
        return;
    }

    switch (request[0])
    {
    case 0x10U:
        if (length != 2U)
        {
            Uds_SendNrc(0x10U, NRC_INCORRECT_LENGTH);
        }
        else if (request[1] != 0x01U)
        {
            Uds_SendNrc(0x10U, NRC_SUBFUNCTION_NOT_SUPPORTED);
        }
        else
        {
            response[0] = 0x50U;
            response[1] = 0x01U;
            IsoTp_Send(response, 2U);
        }
        break;

    case 0x3EU:
        if (length != 2U)
        {
            Uds_SendNrc(0x3EU, NRC_INCORRECT_LENGTH);
        }
        else if (request[1] != 0x00U)
        {
            Uds_SendNrc(0x3EU, NRC_SUBFUNCTION_NOT_SUPPORTED);
        }
        else
        {
            response[0] = 0x7EU;
            response[1] = 0x00U;
            IsoTp_Send(response, 2U);
        }
        break;

    case 0x22U:
    {
        uint16_t did;

        if (length != 3U)
        {
            Uds_SendNrc(0x22U, NRC_INCORRECT_LENGTH);
            break;
        }

        did = ((uint16_t)request[1] << 8U)
            | request[2];

        if (DiagSnapshot_Copy(&snapshot) != pdPASS)
        {
            Uds_SendNrc(0x22U, NRC_CONDITIONS_NOT_CORRECT);
            break;
        }

        response[0] = 0x62U;
        response[1] = request[1];
        response[2] = request[2];

        if (did == 0xF180U)
        {
            response[3] = ECU_SW_MAJOR;
            response[4] = ECU_SW_MINOR;
            response[5] = ECU_SW_PATCH;
            IsoTp_Send(response, 6U);
        }
        else if (did == 0xF181U)
        {
            response[3] = (uint8_t)(snapshot.temperature_x10 >> 8U);
            response[4] = (uint8_t)snapshot.temperature_x10;
            response[5] = snapshot.sensor_valid;
            IsoTp_Send(response, 6U);
        }
        else if (did == 0xF182U)
        {
            response[3] = snapshot.control_mode;
            response[4] = snapshot.fault_bits;
            response[5] = snapshot.app_value;
            response[6] = snapshot.relay_state;
            IsoTp_Send(response, 7U);
        }
        else if (did == 0xF183U)
        {
            PutU32(&response[3], can_tx_success_cnt);
            PutU32(&response[7], can_tx_fail_cnt);
            PutU32(&response[11], can_rx_cnt);
            PutU32(&response[15], can_busoff_cnt);
            IsoTp_Send(response, 19U);
        }
        else
        {
            Uds_SendNrc(0x22U, NRC_REQUEST_OUT_OF_RANGE);
        }
        break;
    }

    case 0x19U:
        if (length < 2U)
        {
            Uds_SendNrc(0x19U, NRC_INCORRECT_LENGTH);
        }
        else if (request[1] == 0x02U)
        {
            uint16_t position = 3U;

            if (length != 3U)
            {
                Uds_SendNrc(0x19U, NRC_INCORRECT_LENGTH);
                break;
            }

            if (DiagSnapshot_Copy(&snapshot) != pdPASS)
            {
                Uds_SendNrc(0x19U, NRC_CONDITIONS_NOT_CORRECT);
                break;
            }

            response[0] = 0x59U;
            response[1] = 0x02U;
            response[2] = 0x01U;

            if ((request[2] & 0x01U) != 0U)
            {
                for (uint8_t bit = 0U; bit < 8U; bit++)
                {
                    if ((snapshot.fault_bits & (1U << bit)) != 0U)
                    {
                        uint16_t code = Uds_DtcCodeFromBit(bit);

                        response[position++] = 0x00U;
                        response[position++] = (uint8_t)(code >> 8U);
                        response[position++] = (uint8_t)code;
                        response[position++] = 0x01U;
                    }
                }
            }

            IsoTp_Send(response, position);
        }
        else if (request[1] == 0x04U)
        {
            NvM_DtcRecord_t record;
            uint16_t code;

            if (length != 6U)
            {
                Uds_SendNrc(0x19U, NRC_INCORRECT_LENGTH);
                break;
            }

            code = ((uint16_t)request[3] << 8U) | request[4];

            if ((request[2] != 0x00U) ||
                (NvM_ReadLatestSetDtc(code, &record) == 0U))
            {
                Uds_SendNrc(0x19U, NRC_REQUEST_OUT_OF_RANGE);
                break;
            }

            response[0] = 0x59U;
            response[1] = 0x04U;
            response[2] = 0x00U;
            response[3] = request[3];
            response[4] = request[4];
            response[5] = record.dtc_status;
            response[6] = request[5];
            response[7] = (uint8_t)(record.temperature_x10 >> 8U);
            response[8] = (uint8_t)record.temperature_x10;
            response[9] = record.control_mode;
            response[10] = record.app_value;
            response[11] = record.relay_state;
            PutU32(&response[12], record.can_error);

            IsoTp_Send(response, 16U);
        }
        else
        {
            Uds_SendNrc(0x19U, NRC_SUBFUNCTION_NOT_SUPPORTED);
        }
        break;

    case 0x14U:
        if (length != 4U)
        {
            Uds_SendNrc(0x14U, NRC_INCORRECT_LENGTH);
        }
        else if ((request[1] != 0xFFU) || (request[2] != 0xFFU) || (request[3] != 0xFFU))
        {
            Uds_SendNrc(0x14U,
                        NRC_REQUEST_OUT_OF_RANGE);
        }
        else if ((DiagSnapshot_Copy(&snapshot) != pdPASS) || (snapshot.fault_bits != 0U))
        {
            Uds_SendNrc(0x14U, NRC_CONDITIONS_NOT_CORRECT);
        }
        else if (NvM_ClearDtcHistory() == 0U)
        {
            Uds_SendNrc(0x14U, NRC_CONDITIONS_NOT_CORRECT);
        }
        else
        {
            response[0] = 0x54U;
            IsoTp_Send(response, 1U);
        }
        break;

    default:
        Uds_SendNrc(request[0], NRC_SERVICE_NOT_SUPPORTED);
        break;
    }
}

static void Uds_EnterProgramming(void)
{
    extern CAN_HandleTypeDef hcan1;

    DiagSnapshot_t snapshot;

    if (DiagSnapshot_Copy(&snapshot) != pdPASS ||
        snapshot.sensor_valid == 0U ||
        snapshot.fault_bits != 0U ||
        (hcan1.Instance->ESR & CAN_ESR_BOFF) != 0U ||
        (xEventGroupGetBits(SystemEventGroup) &
         EVT_FATAL_FAULT) != 0U)
    {
        Uds_SendNrc(0x10U, NRC_CONDITIONS_NOT_CORRECT);
        return;
    }

    xEventGroupClearBits(SystemEventGroup, EVT_UPDATE_ALLOWED);
    xEventGroupSetBits(SystemEventGroup, EVT_UPDATE_REQUESTED);

    EventBits_t events = xEventGroupWaitBits(
        SystemEventGroup,
        EVT_UPDATE_ALLOWED,
        pdFALSE,
        pdTRUE,
        pdMS_TO_TICKS(500)
    );

    if ((events & EVT_UPDATE_ALLOWED) == 0U ||
        DiagSnapshot_Copy(&snapshot) != pdPASS ||
        snapshot.sensor_valid == 0U ||
        snapshot.fault_bits != 0U ||
        (xEventGroupGetBits(SystemEventGroup) &
         EVT_FATAL_FAULT) != 0U)
    {
        xEventGroupClearBits(SystemEventGroup, EVT_UPDATE_REQUESTED | EVT_UPDATE_ALLOWED);

        Uds_SendNrc(0x10U, NRC_CONDITIONS_NOT_CORRECT);
        return;
    }

    /*
     * 还未进入Bootloader，只回答“处理中”。
     * 最终50 02由Bootloader发送。
     */
    uint8_t pending[3] = {0x7FU, 0x10U, 0x78U};

    if (!IsoTp_Send(pending, sizeof(pending)))
    {
        xEventGroupClearBits(SystemEventGroup, EVT_UPDATE_REQUESTED | EVT_UPDATE_ALLOWED);
        return;
    }

    vTaskDelay(pdMS_TO_TICKS(50));

    volatile uint32_t *flag = (volatile uint32_t *)0x2001FFF8UL;            //预留区起点

    flag[0] = 0x55504454UL;
    flag[1] = (uint32_t)~0x55504454UL;

    __DSB();
    NVIC_SystemReset();
}

void Uds_HandleRequest(const uint8_t *request, uint16_t length)
{
    if (length == 0U)
        return;

    if (request[0] == 0x10U && length == 2U && request[1] == 0x02U)
    {
        Uds_EnterProgramming();
        return;
    }

    if (request[0] == 0x22U && length == 3U)
    {
        uint16_t did = ((uint16_t)request[1] << 8) | request[2];
        uint8_t response[7] = {0x62U, request[1], request[2]};

        if (did == 0xF1A0U)
        {
            PutU32(&response[3], ECU_HARDWARE_ID);
            IsoTp_Send(response, 7U);
            return;
        }

        if (did == 0xF1A2U)
        {
            response[3] = 0U;
            IsoTp_Send(response, 4U);
            return;
        }
    }

    if (request[0] == 0x19U && length == 6U &&
        request[1] == 0x04U && request[2] == 0U)
    {
        uint16_t code = ((uint16_t)request[3] << 8) | request[4];

        if (code >= 0x1085U && code <= 0x1087U)
        {
            NvM_DtcRecord_t record;
            uint8_t response[20] = {
                0x59U, 0x04U, 0U, request[3], request[4]
            };

            if (!NvM_ReadLatestSetDtc(code, &record))
            {
                Uds_SendNrc(0x19U, NRC_REQUEST_OUT_OF_RANGE);
                return;
            }

            response[5] = record.dtc_status;
            response[6] = request[5];
            response[7] = (uint16_t)record.temperature_x10 >> 8;
            response[8] = record.temperature_x10;
            response[9] = record.control_mode;
            response[10] = record.app_value;
            response[11] = record.relay_state;
            PutU32(&response[12], record.can_error);
            response[16] = record.voltage_mv >> 8;
            response[17] = record.voltage_mv;
            response[18] = (uint16_t)record.current_ma >> 8;
            response[19] = record.current_ma;

            IsoTp_Send(response, sizeof(response));
            return;
        }
    }

    Uds_HandleNormalRequest(request, length);
}

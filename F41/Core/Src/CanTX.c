#include "CanTX.h"

extern QueueHandle_t Queue_Sensor_TX_t;
extern QueueHandle_t Queue_Sensor_CAN_t;
extern QueueHandle_t Queue_Control_TX_t;
extern QueueHandle_t Queue_CAN_TX_t;

extern CAN_HandleTypeDef hcan1;

extern TaskHandle_t WatchdogTaskHandle;

extern volatile uint32_t can_tx_success_cnt;
extern volatile uint32_t can_tx_fail_cnt;

extern volatile uint32_t can_rx_queue_full_cnt;
extern volatile uint32_t can_tx_queue_full_cnt;
extern volatile uint32_t can_ack_queue_fail_cnt;

BaseType_t CanTX_Enqueue(const Can_Frame_t *frame)
{
    if ((frame == NULL) || (Queue_CAN_TX_t == NULL))
    {
        can_tx_queue_full_cnt++;
        return pdFAIL;
    }

    if (xQueueSend(Queue_CAN_TX_t, frame, 0) != pdPASS)
    {
        can_tx_queue_full_cnt++;
        return pdFAIL;
    }

    return pdPASS;
}

void Trigger_Send_0x280_ACK(uint8_t counter, uint8_t status)
{
    Can_Frame_t frame = {0};

    frame.id  = 0x280U;
    frame.dlc = 8U;

    frame.data[0] = 0x02U;
    frame.data[1] = counter & 0x0FU;
    frame.data[2] = status;
    frame.data[3] = 0U;
    frame.data[4] = 0U;
    frame.data[5] = 0U;
    frame.data[6] = 0U;
    frame.data[7] = Calculate_CRC8_J1850(frame.data, 7U);

    if (CanTX_Enqueue(&frame) != pdPASS)
    {
        can_ack_queue_fail_cnt++;
    }
}

static BaseType_t CanTX_SendFrame(const Can_Frame_t *frame)      //仅该处可发送
{
    CAN_TxHeaderTypeDef tx_header;
    uint32_t tx_mailbox;

    if ((frame == NULL) || (frame->dlc > 8U))
    {
        can_tx_fail_cnt++;
        return pdFAIL;
    }

    if (frame->id > 0x7FFU)
    {
        can_tx_fail_cnt++;
        return pdFAIL;
    }

    tx_header.StdId = frame->id;
    tx_header.ExtId = 0U;
    tx_header.IDE = CAN_ID_STD;
    tx_header.RTR = CAN_RTR_DATA;
    tx_header.DLC = frame->dlc;
    tx_header.TransmitGlobalTime = DISABLE;

    if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) == 0U)
    {
        can_tx_fail_cnt++;
        return pdFAIL;
    }

    if (HAL_CAN_AddTxMessage(&hcan1,
                             &tx_header,
                             (uint8_t *)frame->data,
                             &tx_mailbox) != HAL_OK)
    {
        can_tx_fail_cnt++;
        return pdFAIL;
    }

    /*
     * 只有0x180状态帧需要送给Log任务解析。
     * ACK不需要进入Queue_Sensor_CAN_t。
     */
    if (frame->id == 0x180U)
    {
        xQueueOverwrite(Queue_Sensor_CAN_t, frame);
    }

    return pdPASS;
}

static BaseType_t CanTX_Queue_0x180(void)
{
    Sensor_Sample_t sensor = {0};
    ControlContext_t control = {0};
    SensorMsg_t message = {0};
    Can_Frame_t frame = {0};

    /*
     * 两份快照必须都已经产生，才能生成有效状态帧。
     */
    if (xQueuePeek(Queue_Sensor_TX_t, &sensor, 0) != pdTRUE)
    {
        return pdFAIL;
    }

    if (xQueuePeek(Queue_Control_TX_t, &control, 0) != pdTRUE)
    {
        return pdFAIL;
    }

    frame.id = 0x180U;
    frame.dlc = 8U;

    message.valid_flag = sensor.valid_flag;
    message.app_value = control.app_value;
    message.current_mode = control.current_mode;
    message.fault_bits = control.fault_bits;

    if (sensor.valid_flag == 1U)
    {
        message.temperature = sensor.temperature;
        message.humidity = sensor.humidity;
    }

    Pack_Msg_0x180(&message, frame.data);

    /*
     * 不再直接调用HAL，而是和ACK一样进入统一发送队列。
     */
    return CanTX_Enqueue(&frame);
}

void CanTX_Task(void *pvParameters)
{
    Can_Frame_t frame = {0};

    const TickType_t status_period = pdMS_TO_TICKS(1000);
    TickType_t last_status_time = xTaskGetTickCount();

    for (;;)
    {
        TickType_t current_time = xTaskGetTickCount();

        /*
         * 每隔1秒生成一次0x180状态帧。
         * 这里仅负责打包并入队，不访问HAL。
         */
        if ((current_time - last_status_time) >= status_period)
        {
            last_status_time = current_time;
            (void)CanTX_Queue_0x180();
        }

        /*
         * 有ACK或状态帧进入队列时，最多等待10ms便立即发送。
         * 不再每发送一帧就等待1秒。
         */
        if (xQueueReceive(Queue_CAN_TX_t, &frame, pdMS_TO_TICKS(10)) == pdPASS)
        {
            (void)CanTX_SendFrame(&frame);
        }

        /*
         * CanTX任务持续工作，定期通知看门狗。
         */
        xTaskNotify(WatchdogTaskHandle, HEARTBEAT_CANTX, eSetBits);
    }
}

void HAL_CAN_TxMailbox0CompleteCallback(CAN_HandleTypeDef *hcan)
{
    if (hcan->Instance == CAN1)
    {
        can_tx_success_cnt++;
    }
}

void HAL_CAN_TxMailbox1CompleteCallback(CAN_HandleTypeDef *hcan)
{
    if (hcan->Instance == CAN1)
    {
        can_tx_success_cnt++;
    }
}

void HAL_CAN_TxMailbox2CompleteCallback(CAN_HandleTypeDef *hcan)
{
    if (hcan->Instance == CAN1)
    {
        can_tx_success_cnt++;
    }
}

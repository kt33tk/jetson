#include "CanTX.h"

extern QueueHandle_t Queue_Sensor_TX_t;
extern QueueHandle_t Queue_Sensor_CAN_t;
extern QueueHandle_t Queue_Control_TX_t;

extern CAN_HandleTypeDef hcan1;

extern TaskHandle_t WatchdogTaskHandle;

extern volatile uint32_t can_tx_success_cnt;
extern volatile uint32_t can_tx_fail_cnt;

void Trigger_Send_0x280_ACK(uint8_t rx_counter)
{
    CAN_TxHeaderTypeDef TxHeader;
    uint32_t Tx_Mailbox;
    uint8_t TxData[8] = {0};

    // 1. 配置 0x280 的 CAN 帧头
    TxHeader.StdId = 0x280;
    TxHeader.ExtId = 0;
    TxHeader.IDE = CAN_ID_STD;
    TxHeader.RTR = CAN_RTR_DATA;
    TxHeader.DLC = 8;
    TxHeader.TransmitGlobalTime = DISABLE;

    // 2. 组装负载数据
    TxData[0] = 0x02;        // Echoed ID：用 0x02 代表是被 0x200 指令触发的
    TxData[1] = rx_counter;  // 被拦截的重复 Counter 值
    TxData[2] = 0x01;        // 状态码：0x01 代表 Counter 冲突拒绝执行

    // 3. 计算并附加防伪指纹
    TxData[7] = Calculate_CRC8_J1850(TxData, 7);

    // 4. 调用 HAL 库发送并统计发送状态
    if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) > 0) {
        if (HAL_CAN_AddTxMessage(&hcan1, &TxHeader, TxData, &Tx_Mailbox) == HAL_OK) {

        } else {
            can_tx_fail_cnt++;    // 硬件级发送报错
        }
    } else {
        can_tx_fail_cnt++;        // 邮箱堵塞
    }
}

void CanTX_Task(void *pvParameters)
{
	Can_Frame_t sensor_receive = {0};
	Sensor_Sample_t TX_Receive = {0};
	ControlContext_t control_receive = {0};

	sensor_receive.id = 0x180;
	sensor_receive.dlc = 8;

	TickType_t xLastWaitTime = xTaskGetTickCount();
	const TickType_t xFrequency = pdMS_TO_TICKS(1000);

	//8位数据结构定义：温度高8+温度低8+湿度高8+湿度低8+传感器有效+风扇状态+是否正常
	for(;;)
		{
			CAN_TxHeaderTypeDef TxHeader;
			uint32_t Tx_Mailbox;

			TxHeader.StdId = 0x180;
			TxHeader.ExtId = 0;
			TxHeader.DLC = 8;
			TxHeader.IDE = CAN_ID_STD;
			TxHeader.RTR = CAN_RTR_DATA;
			TxHeader.TransmitGlobalTime = DISABLE;

			if(xQueuePeek(Queue_Sensor_TX_t, &TX_Receive, 0) == pdTRUE)
			{
				if(xQueuePeek(Queue_Control_TX_t, &control_receive, 0) == pdTRUE)
				{
					SensorMsg_t msg_to_send = {0};
					msg_to_send.valid_flag = TX_Receive.valid_flag;
					msg_to_send.app_value = control_receive.app_value;
					msg_to_send.current_mode = control_receive.current_mode;
					msg_to_send.fault_bits = control_receive.fault_bits;

					if(TX_Receive.valid_flag == 1)
					{
						msg_to_send.temperature = TX_Receive.temperature;
						msg_to_send.humidity = TX_Receive.humidity;
					}
					else
					{
						msg_to_send.temperature = 0;
						msg_to_send.humidity = 0;
					}

					Pack_Msg_0x180(&msg_to_send, sensor_receive.data);
				}
			}

			xQueueOverwrite(Queue_Sensor_CAN_t, &sensor_receive);

			if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) > 0)
			{
			    if (HAL_CAN_AddTxMessage(&hcan1, &TxHeader, sensor_receive.data, &Tx_Mailbox) == HAL_OK)
			    {
			        can_tx_success_cnt++; // 报文成功投递给硬件
			    }
			    else
			    {
			        can_tx_fail_cnt++;    // CAN 硬件外设返回 ERROR
			    }
			}
			else
			{
			    can_tx_fail_cnt++;        // 三个发送邮箱全部满载，抛弃当前帧
			}

			xTaskNotify(WatchdogTaskHandle, HEARTBEAT_CANTX, eSetBits);

			vTaskDelayUntil(&xLastWaitTime, xFrequency);
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

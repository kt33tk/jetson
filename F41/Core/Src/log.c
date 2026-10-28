#include "log.h"

extern QueueHandle_t Queue_Sensor_CAN_t;

extern TaskHandle_t SensorTaskHandle;
extern TaskHandle_t ControlTaskHandle;
extern TaskHandle_t LogTaskHandle;
extern TaskHandle_t CanTXTaskHandle;
extern TaskHandle_t WatchdogTaskHandle;

extern volatile uint32_t can_tx_success_cnt;

void Log_Task(void *pvParameters)
{
	Can_Frame_t CAN_Receive = {0};
	uint8_t current_state = 0;

	TickType_t last_monitor_time = xTaskGetTickCount();
	const TickType_t monitor_interval = pdMS_TO_TICKS(5000);

	for(;;)
	{
		TickType_t current_time = xTaskGetTickCount();
		TickType_t time_to_wait = 0;

		if ((current_time - last_monitor_time) < monitor_interval) {
			time_to_wait = monitor_interval - (current_time - last_monitor_time);
		}

		if (xQueueReceive(Queue_Sensor_CAN_t,
		                  &CAN_Receive,
		                  time_to_wait) == pdTRUE)
		{
		    printf("[CAN TX] ID:0x%lX DLC:%d Data: "
		           "%02X %02X %02X %02X %02X %02X %02X %02X\r\n",
		           CAN_Receive.id,
		           CAN_Receive.dlc,
		           CAN_Receive.data[0],
		           CAN_Receive.data[1],
		           CAN_Receive.data[2],
		           CAN_Receive.data[3],
		           CAN_Receive.data[4],
		           CAN_Receive.data[5],
		           CAN_Receive.data[6],
		           CAN_Receive.data[7]);

		    /* 先检查ID和数据长度 */
		    if (CAN_Receive.id != 0x180)
		    {
		        printf("[CAN LOG ERROR] Unexpected ID: 0x%lX\r\n",
		               CAN_Receive.id);
		    }
		    else if (CAN_Receive.dlc != 8)
		    {
		        printf("[CAN LOG ERROR] Invalid DLC: %d\r\n",
		               CAN_Receive.dlc);
		    }
		    /* 再检查CRC */
		    else if (Calculate_CRC8_J1850(CAN_Receive.data, 7)
		             != CAN_Receive.data[7])
		    {
		        printf("[CAN LOG ERROR] CRC mismatch\r\n");
		    }
		    else
		    {
		        SensorMsg_t decoded = {0};

		        /* 使用协议模块统一解包 */
		        Unpack_Msg_0x180(CAN_Receive.data, &decoded);

		        int16_t temp_raw = decoded.temperature;
		        long temp_abs = (temp_raw < 0)
		                      ? -(long)temp_raw
		                      :  (long)temp_raw;

		        printf("Temp: %s%ld.%ld C | Hum: %d | Valid: %d | ",
		               (temp_raw < 0) ? "-" : "",
		               temp_abs / 10,
		               temp_abs % 10,
		               decoded.humidity,
		               decoded.valid_flag);

		        printf("Temp: %s%ld.%ld C | Hum: %d | Valid: %d | "
		               "App: %d | Mode: %d | Counter: %d |  Success: %lu | Fault: 0x%02X\r\n",
					   (temp_raw < 0) ? "-" : "",
					   temp_abs / 10,
					   temp_abs % 10,
		               decoded.humidity,
		               decoded.valid_flag,
		               decoded.app_value,
		               decoded.current_mode,
		               decoded.rx_counter,
					   can_tx_success_cnt,
		               decoded.fault_bits);

		        if (decoded.valid_flag == 0)
		        {
		            printf("[SENSOR ERROR] Sensor data invalid\r\n");
		        }

		        if (decoded.fault_bits != 0)
		        {
		            printf("[CONTROL FAULT] Fault bits: 0x%02X\r\n",
		                   decoded.fault_bits);
		        }

		        if (decoded.app_value > 0U && current_state == 0)
		        {
		            printf("Overload! Progress begin!\r\n");
		            current_state = 1;
		        }
		        else if (decoded.app_value == 0 && current_state == 1)
		        {
		            printf("Work Over\r\n");
		            current_state = 0;
		        }
		    }
		}

		// ==========================================
		// 业务 2：处理系统资源监控 (定时 5 秒触发)
		// ==========================================
		// 每次由于接收到数据或超时醒来后，都检查一下是不是够 5 秒了
		current_time = xTaskGetTickCount();
		if( (current_time - last_monitor_time) >= monitor_interval )
		{
			// 更新时间戳，重新开始下一个 5 秒倒计时
			last_monitor_time = current_time;

			size_t free_heap = xPortGetFreeHeapSize();
			size_t min_ever_heap = xPortGetMinimumEverFreeHeapSize();

			UBaseType_t sensor_stack = uxTaskGetStackHighWaterMark(SensorTaskHandle);
			UBaseType_t control_stack = uxTaskGetStackHighWaterMark(ControlTaskHandle);
			UBaseType_t log_stack = uxTaskGetStackHighWaterMark(LogTaskHandle);
			UBaseType_t CanTX_stack = uxTaskGetStackHighWaterMark(CanTXTaskHandle);
			UBaseType_t Watchdog_stack = uxTaskGetStackHighWaterMark(WatchdogTaskHandle);

			printf("\r\n==== Resource Monitor ====\r\n");

			printf("CAN Bus - TX OK: %lu | TX FAIL: %lu | RX: %lu | BUS-OFF: %lu\r\n",
						       can_tx_success_cnt, can_tx_fail_cnt, can_rx_cnt, can_busoff_cnt);

			printf("CAN Queue - RX FULL:%lu | TX FULL:%lu | ACK QUEUE FAIL:%lu\r\n",
			       can_rx_queue_full_cnt,
			       can_tx_queue_full_cnt,
			       can_ack_queue_fail_cnt);

			printf("CAN Error - ACK:%lu | BIT:%lu | Last:0x%08lX | TEC:%u | REC:%u\r\n",
			       can_ack_error_cnt,
			       can_bit_error_cnt,
			       can_last_error,
			       can_last_tec,
			       can_last_rec);

			printf("Heap - Current Free: %u Bytes | Min Ever Free: %u Bytes\r\n", free_heap, min_ever_heap);
			// 【建议修改】把 Watchdog 的剩余堆栈也加到这行综合打印里
			printf("Stack (Words) - Sensor: %lu | Control: %lu | Log: %lu | CanTX: %lu | Watchdog: %lu\r\n",
							sensor_stack, control_stack, log_stack, CanTX_stack, Watchdog_stack);

			if (Watchdog_stack < 32)
			{
				printf("[ALARM] WatchdogTask Stack lower than 32 words!\r\n");
			}
			if (sensor_stack < 32) printf("[ALARM] SensorTask Stack < 32!\r\n");
			if (control_stack < 32) printf("[ALARM] ControlTask Stack < 32!\r\n");
			if (log_stack < 32) printf("[ALARM] LogTask Stack < 32!\r\n");
			if (CanTX_stack < 32) printf("[ALARM] CanTXTask Stack < 32!\r\n");
		}
	}
}

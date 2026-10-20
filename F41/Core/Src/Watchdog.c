#include "Watchdog.h"

extern TaskHandle_t WatchdogTaskHandle;
extern IWDG_HandleTypeDef hiwdg;

void Watchdog_Task(void *pvParameters)
{
	TickType_t last_control_tick = xTaskGetTickCount();
	TickType_t last_cantx_tick   = xTaskGetTickCount();
	TickType_t last_sensor_tick  = xTaskGetTickCount();
	uint8_t system_fatal_fault = 0;

	for(;;)
	{
		vTaskDelay(pdMS_TO_TICKS(1000));

		uint32_t notified_value = 0;

		xTaskNotifyWait(0x00, 0xFFFFFFFF, &notified_value, 0);
		TickType_t current_tick = xTaskGetTickCount();

		if((notified_value & HEARTBEAT_CONTROL) != 0)
		{
			last_control_tick = current_tick;
		}
		if((notified_value & HEARTBEAT_CANTX) != 0)
		{
			last_cantx_tick = current_tick;
		}
		if((notified_value & HEARTBEAT_SENSOR) != 0)
		{
			last_sensor_tick = current_tick;
		}

		if(system_fatal_fault == 0)
		{
			if ((current_tick - last_control_tick) > pdMS_TO_TICKS(1500))
			{
			    system_fatal_fault = 1;
			}
			else if ((current_tick - last_cantx_tick) > pdMS_TO_TICKS(1500))
			{
			    system_fatal_fault = 1;
			}
			else if ((current_tick - last_sensor_tick) > pdMS_TO_TICKS(3000))
			{
			    system_fatal_fault = 1;
			}
			else
			{
				HAL_IWDG_Refresh(&hiwdg);
			}
		}
	}
}

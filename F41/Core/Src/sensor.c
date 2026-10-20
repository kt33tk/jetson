#include "sensor.h"

extern QueueHandle_t Queue_Sensor_TX_t;
extern QueueHandle_t Queue_Sensor_Control_t;

extern TaskHandle_t WatchdogTaskHandle;

void Sensor_Task(void *pvParameters)
{
	TickType_t xLastWaitTime;
	xLastWaitTime = xTaskGetTickCount();
	const TickType_t xFrequentTime = pdMS_TO_TICKS(1000);

	Sensor_Sample_t sensor_now = {0};
	sensor_now.temperature = 25;
	sensor_now.humidity = 10;
	sensor_now.valid_flag = 1;
	sensor_now.timestamp = xTaskGetTickCount();

	int16_t count = 0;

	for(;;)
	{
		count ++;

		if(count > 0 && count < 16)
		{
			sensor_now.temperature ++;
		}
		else if(count == 16)
		{
			count = -30;
		}
		else if(count <= 0)
		{
			sensor_now.temperature --;
		}

		sensor_now.timestamp = xTaskGetTickCount();

		xQueueOverwrite(Queue_Sensor_TX_t, &sensor_now);
		xQueueOverwrite(Queue_Sensor_Control_t, &sensor_now);

		xTaskNotify(WatchdogTaskHandle, HEARTBEAT_SENSOR, eSetBits);

		vTaskDelayUntil(&xLastWaitTime, xFrequentTime);
	}
}

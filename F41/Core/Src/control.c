#include "control.h"

extern QueueHandle_t Queue_Sensor_Control_t;
extern QueueHandle_t Queue_Control_TX_t;
extern QueueHandle_t Queue_Control_Cmd_t;

extern TaskHandle_t WatchdogTaskHandle;

void Control_Task(void *pvParameters)
{
	Sensor_Sample_t Control_Receive = {0};
	ControlContext_t ctx = {0};
	ControlCommand_t cmd = {0};

	ControlCore_Init(&ctx, xTaskGetTickCount());

	for(;;)
	{
		if(xQueueReceive(Queue_Sensor_Control_t, &Control_Receive, pdMS_TO_TICKS(1500)) == pdTRUE)
		{

		}
		else
		{
			Control_Receive.valid_flag = 0;
		}

		if(xQueueReceive(Queue_Control_Cmd_t, &cmd, 0) == pdTRUE)
		{
			if(cmd.has_new_cmd == 1)
			{
				if(cmd.target_mode == 2)
				{
					HAL_GPIO_WritePin(MODE2_GPIO_Port, MODE2_Pin, GPIO_PIN_SET);
				}

				if(cmd.manual_request == 1)
				{
					HAL_GPIO_WritePin(VALUE_GPIO_Port, VALUE_Pin, GPIO_PIN_SET);
				}
			}

			cmd.has_new_cmd = 0;
		}
		else
		{
		    // 没有收到命令，清空标志位，防止把旧命令重复执行
			cmd.has_new_cmd = 0;
		}

		ControlCore_Step(&ctx, &Control_Receive, &cmd, xTaskGetTickCount());

		if(ctx.app_value == 1)
		{
			HAL_GPIO_WritePin(LED_Control_GPIO_Port, LED_Control_Pin, GPIO_PIN_SET);
		}
		else
		{
			HAL_GPIO_WritePin(LED_Control_GPIO_Port, LED_Control_Pin, GPIO_PIN_RESET);
		}

		xTaskNotify(WatchdogTaskHandle, HEARTBEAT_CONTROL, eSetBits);

		xQueueOverwrite(Queue_Control_TX_t, &ctx);
	}
}

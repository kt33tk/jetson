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

		memset(&cmd, 0, sizeof(cmd));

		if (xQueueReceive(
		        Queue_Control_Cmd_t,
		        &cmd,
		        0) != pdTRUE)
		{
		    cmd.valid = 0;
		}

		/* 注意：此处之前绝不能清除cmd.valid */
		ControlCore_Step(
		    &ctx,
		    &Control_Receive,
		    &cmd,
		    xTaskGetTickCount()
		);

		if (ctx.app_value > 0U)
		{
		    HAL_GPIO_WritePin(
		        LED_Control_GPIO_Port,
		        LED_Control_Pin,
		        GPIO_PIN_SET
		    );
		}
		else
		{
		    HAL_GPIO_WritePin(
		        LED_Control_GPIO_Port,
		        LED_Control_Pin,
		        GPIO_PIN_RESET
		    );
		}

		xTaskNotify(WatchdogTaskHandle, HEARTBEAT_CONTROL, eSetBits);

		xQueueOverwrite(Queue_Control_TX_t, &ctx);
	}
}

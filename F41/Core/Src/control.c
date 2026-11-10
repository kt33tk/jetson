#include "control.h"

extern QueueHandle_t Queue_Sensor_Control_t;
extern QueueHandle_t Queue_Control_TX_t;
extern QueueHandle_t Queue_Control_Cmd_t;
extern QueueHandle_t Queue_Control_Config_t;

extern TaskHandle_t WatchdogTaskHandle;

void Control_Task(void *pvParameters)
{
	const NvM_ParamPayload_t *startup_params = (const NvM_ParamPayload_t *)pvParameters;

	Sensor_Sample_t Control_Receive = {0};
	ControlContext_t ctx = {0};
	ControlCommand_t cmd = {0};
	uint8_t reported_fault_bits = 0U;
	BaseType_t sensor_receive_result = pdFALSE;

	ControlCore_Init(&ctx, xTaskGetTickCount());

	if (startup_params != NULL)
	{
	    ControlCore_ApplyConfig(&ctx, startup_params->auto_on_temp_x10, startup_params->auto_off_temp_x10, startup_params->manual_timeout_ms, startup_params->sensor_timeout_ms);
	}

	for(;;)
	{
		NvM_ParamPayload_t updated_params;

		if (xQueueReceive(Queue_Control_Config_t, &updated_params, 0U) == pdTRUE)
		{
		    ControlCore_ApplyConfig(&ctx, updated_params.auto_on_temp_x10, updated_params.auto_off_temp_x10, updated_params.manual_timeout_ms, updated_params.sensor_timeout_ms);
		}

		sensor_receive_result = xQueueReceive(Queue_Sensor_Control_t, &Control_Receive, pdMS_TO_TICKS(100));

		memset(&cmd, 0, sizeof(cmd));

		if (xQueueReceive(Queue_Control_Cmd_t, &cmd, 0) != pdTRUE)
		{
		    cmd.valid = 0;
		}

		/* 注意：此处之前绝不能清除cmd.valid */
		ControlCore_Step(&ctx, &Control_Receive, (sensor_receive_result == pdTRUE) ? 1U : 0U, &cmd, xTaskGetTickCount());

		uint8_t current_fault_bits = ctx.fault_bits;

		uint8_t changed_bits = reported_fault_bits ^ current_fault_bits;

		for (uint8_t bit = 0U; bit < 8U; bit++)
		{
		    uint8_t mask = (uint8_t)(1U << bit);

		    if ((changed_bits & mask) == 0U)
		    {
		        continue;
		    }

		    NvM_DtcRequestData_t dtc;
		    memset(&dtc, 0, sizeof(dtc));

		    if ((current_fault_bits & mask) != 0U)
		    {
		        dtc.event = NVM_DTC_EVENT_SET;
		        dtc.dtc_status = 1U;
		    }
		    else
		    {
		        dtc.event = NVM_DTC_EVENT_CLEAR;
		        dtc.dtc_status = 0U;
		    }

		    switch (bit)
		    {
		    case 0U:
		        dtc.dtc_code = 0x1001U;
		        break;

		    case 1U:
		        dtc.dtc_code = 0x1002U;
		        break;

		    case 2U:
		        dtc.dtc_code = 0x1003U;
		        break;

		    case 3U:
		        dtc.dtc_code = 0x1004U;
		        break;

		    default:
		        dtc.dtc_code =
		            (uint16_t)(0x1080U + bit);
		        break;
		    }

		    dtc.control_mode = (uint8_t)ctx.current_mode;

		    dtc.temperature_x10 = Control_Receive.temperature;

		    dtc.app_value = ctx.app_value;
		    dtc.relay_state = ctx.relay_state;
		    dtc.timestamp = xTaskGetTickCount();
		    dtc.can_error = can_last_error;

		    if (NvM_RequestDtc(&dtc) != 0U)
		    {
		        if ((current_fault_bits & mask) != 0U)
		        {
		            reported_fault_bits |= mask;
		        }
		        else
		        {
		            reported_fault_bits &= (uint8_t)(~mask);
		        }
		    }
		}

		if (ctx.current_mode == CTRL_MODE_SAFE)
		{
		    xEventGroupSetBits(SystemEventGroup, EVT_CONTROL_SAFE);
		}
		else
		{
		    xEventGroupClearBits(SystemEventGroup, EVT_CONTROL_SAFE);
		}

		DiagSnapshot_UpdateControl( (uint8_t)ctx.current_mode, ctx.cmd_value, ctx.app_value, ctx.relay_state, ctx.fault_bits, xTaskGetTickCount());

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

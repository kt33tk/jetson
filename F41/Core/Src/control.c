#include "control.h"
#include "FanControl.h"

extern QueueHandle_t Queue_Sensor_Control_t;
extern QueueHandle_t Queue_Control_TX_t;
extern QueueHandle_t Queue_Control_Cmd_t;
extern QueueHandle_t Queue_Control_Config_t;

extern TaskHandle_t WatchdogTaskHandle;

void Control_Task(void *pvParameters)
{
    const NvM_ParamPayload_t *startup_params = pvParameters;

    Sensor_Sample_t sensor = {0};
    ControlContext_t ctx = {0};
    ControlCommand_t cmd = {0};

    uint8_t reported_fault_bits = 0U;

    ControlCore_Init(&ctx, xTaskGetTickCount());

    if (startup_params != NULL)
    {
        ControlCore_ApplyConfig(
            &ctx,
            startup_params->auto_on_temp_x10,
            startup_params->auto_off_temp_x10,
            startup_params->manual_timeout_ms,
            startup_params->sensor_timeout_ms
        );
    }

    for (;;)
    {
        NvM_ParamPayload_t params;

        if (xQueueReceive(Queue_Control_Config_t,
                          &params, 0U) == pdTRUE)
        {
            ControlCore_ApplyConfig(
                &ctx,
                params.auto_on_temp_x10,
                params.auto_off_temp_x10,
                params.manual_timeout_ms,
                params.sensor_timeout_ms
            );
        }

        BaseType_t sensor_updated = xQueueReceive(
            Queue_Sensor_Control_t,
            &sensor,
            pdMS_TO_TICKS(20)
        );

        memset(&cmd, 0, sizeof(cmd));
        xQueueReceive(Queue_Control_Cmd_t, &cmd, 0U);

        ControlCore_Step(
            &ctx,
            &sensor,
            sensor_updated == pdTRUE,
            &cmd,
            xTaskGetTickCount()
        );

        EventBits_t events = xEventGroupGetBits(SystemEventGroup);
        uint8_t update_requested = (events & EVT_UPDATE_REQUESTED) != 0U;

        /*
         * 升级请求保持期间，不允许其他命令重新开启输出。
         * 实际输出仍由Control任务处理。
         */
        if (update_requested)
        {
            ctx.current_mode = CTRL_MODE_SAFE;
            ctx.cmd_value = 0U;
            ctx.app_value = 0U;
            ctx.relay_state = 0U;
        }

        if (FanControl_Update(ctx.app_value,  xTaskGetTickCount()) != 0U)
        {
            ctx.fault_bits |= 0x10U;
            ctx.current_mode = CTRL_MODE_SAFE;
            ctx.cmd_value = 0U;
            ctx.app_value = 0U;

            FanControl_Update(0U, xTaskGetTickCount());
        }

        uint8_t current_fault_bits = ctx.fault_bits;
        uint8_t changed_bits = reported_fault_bits ^ current_fault_bits;

        for (uint8_t bit = 0U; bit < 8U; bit++)
        {
            uint8_t mask = (uint8_t)(1U << bit);

            if ((changed_bits & mask) == 0U)
                continue;

            NvM_DtcRequestData_t dtc = {0};

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

            dtc.dtc_code = bit < 4U ? (uint16_t)(0x1001U + bit) : (uint16_t)(0x1080U + bit);

            dtc.control_mode = ctx.current_mode;
            dtc.temperature_x10 = sensor.temperature;
            dtc.app_value = ctx.app_value;
            dtc.relay_state = ctx.relay_state;
            dtc.timestamp = xTaskGetTickCount();
            dtc.can_error = can_last_error;

            if (NvM_RequestDtc(&dtc))
            {
                if ((current_fault_bits & mask) != 0U)
                    reported_fault_bits |= mask;
                else
                    reported_fault_bits &= (uint8_t)~mask;
            }
        }

        if (ctx.current_mode == CTRL_MODE_SAFE)
            xEventGroupSetBits(SystemEventGroup, EVT_CONTROL_SAFE);
        else
            xEventGroupClearBits(SystemEventGroup, EVT_CONTROL_SAFE);

        HAL_GPIO_WritePin(LED_Control_GPIO_Port, LED_Control_Pin, ctx.app_value > 0U ? GPIO_PIN_SET : GPIO_PIN_RESET);

        DiagSnapshot_UpdateControl(
            ctx.current_mode,
            ctx.cmd_value,
            ctx.app_value,
            ctx.relay_state,
            ctx.fault_bits,
            xTaskGetTickCount()
        );

        /*
         * 风扇停止动作执行之后，才允许升级。
         */
        events = xEventGroupGetBits(SystemEventGroup);

        if (update_requested &&
            ctx.fault_bits == 0U &&
            ctx.app_value == 0U &&
            ctx.relay_state == 0U &&
            (events & EVT_FATAL_FAULT) == 0U)
        {
            xEventGroupSetBits(SystemEventGroup, EVT_UPDATE_ALLOWED);
        }
        else
        {
            xEventGroupClearBits(SystemEventGroup, EVT_UPDATE_ALLOWED);
        }

        xQueueOverwrite(Queue_Control_TX_t, &ctx);

        xTaskNotify(WatchdogTaskHandle, HEARTBEAT_CONTROL, eSetBits);
    }
}

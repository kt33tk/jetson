#include "control.h"
#include "FanControl.h"

extern QueueHandle_t Queue_Sensor_Control_t;
extern QueueHandle_t Queue_Control_TX_t;
extern QueueHandle_t Queue_Control_Cmd_t;
extern QueueHandle_t Queue_Control_Config_t;

extern TaskHandle_t WatchdogTaskHandle;

static void PowerControl_Step(ControlContext_t *ctx, const Sensor_Sample_t *sensor, uint8_t clear_requested, uint32_t now)
{
    ctx->voltage_mv = sensor->voltage_mv;
    ctx->current_ma = sensor->current_ma;
    ctx->power_status = sensor->power_status;

    if ((now - sensor->timestamp) > pdMS_TO_TICKS(300))
        ctx->power_status = POWER_STALE;

    if (ctx->power_status != POWER_OK)
    {
        ctx->fault_bits |= FAULT_INA226;
        ctx->voltage_mv = 0xFFFFU;
        ctx->current_ma = INT16_MIN;
    }
    else
    {
        int32_t current = ctx->current_ma;
        if (current < 0)
            current = -current;

        ctx->fault_bits &= (uint8_t)~FAULT_INA226;

        if (ctx->voltage_mv > 5500U)
            ctx->fault_bits |= FAULT_OVERVOLTAGE;                   //设置对应故障位
        else if (ctx->voltage_mv <= 5200U)
            ctx->fault_bits &= (uint8_t)~FAULT_OVERVOLTAGE;         //清除对应故障位

        if (current > 30)
            ctx->fault_bits |= FAULT_OVERCURRENT;
        else if (clear_requested)
            ctx->fault_bits &= (uint8_t)~FAULT_OVERCURRENT;
    }

    ctx->relay_state =
        (ctx->current_mode == CTRL_MODE_AUTO ||
         ctx->current_mode == CTRL_MODE_MANUAL) &&
        (ctx->fault_bits & 0xE0U) == 0U;
}

void Control_Task(void *pvParameters)
{
    const NvM_ParamPayload_t *startup_params = pvParameters;
    Sensor_Sample_t sensor = {.power_status = POWER_I2C_ERROR};
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

    /* 修改1：等待首条实际采样，避免上电先记录一次假故障。 */
    xQueuePeek(Queue_Sensor_Control_t, &sensor, portMAX_DELAY);

    for (;;)
    {
        NvM_ParamPayload_t params;

        if (xQueueReceive(Queue_Control_Config_t, &params, 0U) == pdTRUE)
        {
            ControlCore_ApplyConfig(
                &ctx,
                params.auto_on_temp_x10,
                params.auto_off_temp_x10,
                params.manual_timeout_ms,
                params.sensor_timeout_ms
            );
        }

        BaseType_t sensor_updated = xQueueReceive(Queue_Sensor_Control_t, &sensor, pdMS_TO_TICKS(20));

        memset(&cmd, 0, sizeof(cmd));
        xQueueReceive(Queue_Control_Cmd_t, &cmd, 0U);

        uint8_t clear_requested = cmd.valid && cmd.command == CONTROL_CMD_CLEAR_FAULT;

        /*
         * 修改2：保留过压滞回状态和过流锁存状态。
         * 两者均交给PowerControl_Step决定何时解除。
         */
        uint8_t electrical_latched =ctx.fault_bits & (FAULT_OVERVOLTAGE | FAULT_OVERCURRENT);

        uint32_t now = xTaskGetTickCount();

        ControlCore_Step(&ctx, &sensor, sensor_updated == pdTRUE, &cmd, now);

        ctx.fault_bits |= electrical_latched;

        EventBits_t events = xEventGroupGetBits(SystemEventGroup);
        uint8_t update_requested = (events & EVT_UPDATE_REQUESTED) != 0U;

        if (update_requested)
        {
            ctx.current_mode = CTRL_MODE_SAFE;
            ctx.cmd_value = 0U;
            ctx.app_value = 0U;
        }

        if (FanControl_Update(ctx.app_value, now))
        {
            ctx.fault_bits |= 0x10U;
            ctx.current_mode = CTRL_MODE_SAFE;
            ctx.cmd_value = 0U;
            ctx.app_value = 0U;
            FanControl_Update(0U, now);
        }

        PowerControl_Step(&ctx, &sensor, clear_requested, now);

        HAL_GPIO_WritePin(
            RELAY_IN_GPIO_Port, RELAY_IN_Pin,
            ctx.relay_state ? GPIO_PIN_RESET : GPIO_PIN_SET
        );

        uint8_t changed = reported_fault_bits ^ ctx.fault_bits;

        for (uint8_t bit = 0U; bit < 8U; bit++)
        {
            uint8_t mask = 1U << bit;

            if ((changed & mask) == 0U)
                continue;

            NvM_DtcRequestData_t dtc = {0};

            dtc.dtc_status = (ctx.fault_bits & mask) != 0U;
            dtc.event = dtc.dtc_status ?
                        NVM_DTC_EVENT_SET : NVM_DTC_EVENT_CLEAR;

            dtc.dtc_code = bit < 4U ?
                          (uint16_t)(0x1001U + bit) :
                          (uint16_t)(0x1080U + bit);

            dtc.control_mode = ctx.current_mode;
            dtc.temperature_x10 = sensor.temperature;
            dtc.app_value = ctx.app_value;
            dtc.relay_state = ctx.relay_state;
            dtc.timestamp = now;
            dtc.can_error = can_last_error;
            dtc.voltage_mv = ctx.voltage_mv;
            dtc.current_ma = ctx.current_ma;

            if (NvM_RequestDtc(&dtc))
            {
                if (dtc.dtc_status)
                    reported_fault_bits |= mask;
                else
                    reported_fault_bits &= (uint8_t)~mask;
            }
        }

        if (ctx.current_mode == CTRL_MODE_SAFE)
            xEventGroupSetBits(SystemEventGroup, EVT_CONTROL_SAFE);
        else
            xEventGroupClearBits(SystemEventGroup, EVT_CONTROL_SAFE);

        HAL_GPIO_WritePin(
            LED_Control_GPIO_Port, LED_Control_Pin,
            ctx.app_value > 0U ? GPIO_PIN_SET : GPIO_PIN_RESET
        );

        DiagSnapshot_UpdateControl(
            ctx.current_mode,
            ctx.cmd_value,
            ctx.app_value,
            ctx.relay_state,
            ctx.fault_bits,
            now
        );

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

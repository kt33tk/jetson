#include "ControlCore.h"

void ControlCore_Init(ControlContext_t *ctx, uint32_t current_time)
{
	ctx->current_mode = CTRL_MODE_INIT;
	ctx->cmd_value = 0;
	ctx->app_value = 0;
	ctx->fault_bits = 0;
	ctx->init_start_time = current_time;
	ctx->valid_sample_cnt = 0;
	ctx->last_sensor_time = current_time;
	ctx->last_cmd_time = current_time;
	ctx->relay_state = 0U;
	ctx->auto_on_temp_x10 = 300;
	ctx->auto_off_temp_x10 = 280;
	ctx->manual_timeout_ms = 2000U;
	ctx->sensor_timeout_ms = 2000U;
}

void ControlCore_ApplyConfig(ControlContext_t *ctx, int16_t auto_on_temp_x10, int16_t auto_off_temp_x10, uint32_t manual_timeout_ms, uint32_t sensor_timeout_ms)
{
    if (ctx == NULL)
    {
        return;
    }

    if (auto_off_temp_x10 >= auto_on_temp_x10)
    {
        return;
    }

    ctx->auto_on_temp_x10 = auto_on_temp_x10;
    ctx->auto_off_temp_x10 = auto_off_temp_x10;
    ctx->manual_timeout_ms = manual_timeout_ms;
    ctx->sensor_timeout_ms = sensor_timeout_ms;
}

void ControlCore_Step(ControlContext_t *ctx, Sensor_Sample_t *sensor, uint8_t sensor_updated, ControlCommand_t *cmd, uint32_t current_time)
{
	/*
	 * 只有真正收到一条新传感器消息时，
	 * 才检查这条消息是否有效。
	 */
	if (sensor_updated != 0U)
	{
	    if (sensor->valid_flag == 1U)
	    {
	        /*
	         * 收到新的有效样本。
	         * 记录最后一次有效样本时间。
	         */
	        ctx->last_sensor_time = current_time;
	    }
	    else if (ctx->current_mode != CTRL_MODE_INIT)
	    {
	        /*
	         * 确实收到了一条新消息，
	         * 但消息明确表示传感器无效。
	         */
	        ctx->current_mode = CTRL_MODE_SAFE;
	        ctx->fault_bits |= 0x01U;
	    }
	}

	/*
	 * AUTO或MANUAL运行期间，
	 * 长时间没有收到新的有效传感器样本。
	 */
	if (((ctx->current_mode == CTRL_MODE_AUTO) || (ctx->current_mode == CTRL_MODE_MANUAL)) && ((current_time - ctx->last_sensor_time) >= ctx->sensor_timeout_ms))
	{
	    ctx->current_mode = CTRL_MODE_SAFE;
	    ctx->fault_bits |= 0x04U;
	}

	switch(ctx->current_mode){
		case CTRL_MODE_INIT:
			ctx->app_value = 0;
			if (sensor_updated != 0U)
			{
			    if (sensor->valid_flag == 1U)
			    {
			        ctx->valid_sample_cnt++;

			        if (ctx->valid_sample_cnt >= 3U)
			        {
			            ctx->current_mode = CTRL_MODE_AUTO;
			            ctx->valid_sample_cnt = 0U;
			        }
			    }
			    else
			    {
			        /*
			         * 收到了一条新的无效样本，
			         * 因此连续有效样本重新计数。
			         */
			        ctx->valid_sample_cnt = 0U;
			    }
			}

			if((current_time - ctx->init_start_time) > INIT_TIMEOUT_MS)
			{
				ctx->current_mode = CTRL_MODE_SAFE;
				ctx->fault_bits |= 0x02;
			}

			if (cmd != NULL && cmd->valid == 1U)
			{
			    if (cmd->command == CONTROL_CMD_SAFE)
			    {
			        ctx->current_mode = CTRL_MODE_SAFE;
			        ctx->app_value = 0;
			        ctx->cmd_value = 0;
			        break;
			    }

			    /*
			     * INIT期间的AUTO、MANUAL、CLEAR已经进入状态机，
			     * 但根据安全规则暂不执行。
			     */
			}

			break;

		case CTRL_MODE_AUTO:
			if ((sensor_updated != 0U) && (sensor->valid_flag == 1U))
			{
			    if (sensor->temperature >= ctx->auto_on_temp_x10)
			    {
			        ctx->cmd_value = 100U;
			        ctx->app_value = 100U;
			    }
			    else if (sensor->temperature <= ctx->auto_off_temp_x10)
			    {
			        ctx->cmd_value = 0U;
			        ctx->app_value = 0U;
			    }
			}

			if (cmd != NULL && cmd->valid == 1U)
			{
			    switch (cmd->command)
			    {
			        case CONTROL_CMD_AUTO:
			            /* 已经在AUTO，保持不变 */
			            break;

			        case CONTROL_CMD_MANUAL:
			            ctx->current_mode = CTRL_MODE_MANUAL;
			            ctx->app_value = cmd->target_percent;
			            ctx->cmd_value = cmd->target_percent;
			            ctx->last_cmd_time = current_time;
			            break;

			        case CONTROL_CMD_SAFE:
			            ctx->current_mode = CTRL_MODE_SAFE;
			            ctx->app_value = 0;
			            ctx->cmd_value = 0;
			            break;

			        case CONTROL_CMD_CLEAR_FAULT:
			            /* AUTO状态不接受CLEAR */
			            break;

			        default:
			            break;
			    }
			}

			break;

		case CTRL_MODE_MANUAL:
			if (cmd != NULL && cmd->valid == 1U)
			{
			    switch (cmd->command)
			    {
			        case CONTROL_CMD_AUTO:
			            ctx->current_mode = CTRL_MODE_AUTO;
			            ctx->app_value = 0;
			            ctx->cmd_value = 0;
			            break;

			        case CONTROL_CMD_MANUAL:
			            ctx->app_value = cmd->target_percent;
			            ctx->cmd_value = cmd->target_percent;
			            ctx->last_cmd_time = current_time;
			            break;

			        case CONTROL_CMD_SAFE:
			            ctx->current_mode = CTRL_MODE_SAFE;
			            ctx->app_value = 0;
			            ctx->cmd_value = 0;
			            break;

			        case CONTROL_CMD_CLEAR_FAULT:
			            /* MANUAL状态不接受CLEAR */
			            break;

			        default:
			            break;
			    }
			}

			if (ctx->current_mode == CTRL_MODE_MANUAL && (current_time - ctx->last_cmd_time) >= ctx->manual_timeout_ms)
			{
			    ctx->current_mode = CTRL_MODE_SAFE;
			    ctx->fault_bits |= 0x08U;
			    ctx->app_value = 0;
			    ctx->cmd_value = 0;
			}

			break;

		case CTRL_MODE_SAFE:
			ctx->cmd_value = 0;
			ctx->app_value = 0;

			if (cmd != NULL && cmd->valid == 1U)
			{
			    switch (cmd->command)
			    {
			        case CONTROL_CMD_CLEAR_FAULT:
			            if ((sensor->valid_flag == 1U) &&
			                ((current_time -
			                  ctx->last_sensor_time) < ctx->sensor_timeout_ms))
			            {
			                ctx->fault_bits = 0;
			                ctx->init_start_time = current_time;
			                ctx->valid_sample_cnt = 0;
			                ctx->current_mode = CTRL_MODE_INIT;
			            }
			            break;

			        case CONTROL_CMD_SAFE:
			            /* 已经在SAFE，保持 */
			            break;

			        case CONTROL_CMD_AUTO:
			        case CONTROL_CMD_MANUAL:
			            /* SAFE状态下禁止直接恢复运行 */
			            break;

			        default:
			            break;
			    }
			}

			break;
	}

	if (cmd != NULL)
	{
		cmd->valid = 0U;
	}
}

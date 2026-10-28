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
}

void ControlCore_Step(ControlContext_t *ctx, Sensor_Sample_t *sensor, ControlCommand_t *cmd, uint32_t current_time)
{
	if (sensor->valid_flag == 1U)
	{
	    ctx->last_sensor_time = current_time;
	}
	else if (ctx->current_mode != CTRL_MODE_INIT)
	{
	    ctx->current_mode = CTRL_MODE_SAFE;
	    ctx->fault_bits |= 0x01U;
	}

	switch(ctx->current_mode){
		case CTRL_MODE_INIT:
			ctx->app_value = 0;
			if(sensor->valid_flag == 1)
			{
				ctx->valid_sample_cnt ++;

				if(ctx->valid_sample_cnt >= 3)
				{
					ctx->current_mode = CTRL_MODE_AUTO;
					ctx->valid_sample_cnt = 0;
				}
			}
			else
			{
				ctx->valid_sample_cnt = 0;
			}

			if((current_time - ctx->init_start_time) > 8000)
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
			if (sensor->temperature >= 300)
			{
				ctx->cmd_value = 100U;
				ctx->app_value = 100U;
			}
			else if (sensor->temperature <= 280)
			{
				ctx->cmd_value = 0U;
				ctx->app_value = 0U;
			}

			if((current_time - ctx->last_sensor_time) >= 2000)
			{
				ctx->current_mode = CTRL_MODE_SAFE;
				ctx->fault_bits |= 0x04;
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

			if (ctx->current_mode == CTRL_MODE_MANUAL &&
			    (current_time - ctx->last_cmd_time) >= 2000U)
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
			                  ctx->last_sensor_time) < 2000U))
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

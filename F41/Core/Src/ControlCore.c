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
}

void ControlCore_Step(ControlContext_t *ctx, Sensor_Sample_t *sensor, ControlCommand_t *cmd, uint32_t current_time)
{
	if(sensor->valid_flag == 1)
	{
		ctx->last_sensor_time = current_time;
	}
	else
	{
		ctx->current_mode = CTRL_MODE_SAFE;
		ctx->fault_bits |= 0x01;
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
			break;

		case CTRL_MODE_AUTO:
			if(sensor->temperature >= 30)
			{
				ctx->app_value = 1;
				ctx->cmd_value = 1;
			}
			else if(sensor->temperature <= 28)
			{
				ctx->app_value = 0;
				ctx->cmd_value = 0;
			}

			if((current_time - ctx->last_sensor_time) >= 2000)
			{
				ctx->current_mode = CTRL_MODE_SAFE;
				ctx->fault_bits |= 0x04;
			}

			if(cmd != 0 && cmd->has_new_cmd == 1)
			{
				if(cmd->target_mode == CTRL_MODE_MANUAL)
				{
					ctx->current_mode = CTRL_MODE_MANUAL;
					ctx->app_value = cmd->manual_request;
					ctx->cmd_value = cmd->manual_request;
					cmd->has_new_cmd = 0;
					ctx->last_cmd_time = current_time;
					break;
				}
			}

			break;

		case CTRL_MODE_MANUAL:
			if(cmd != 0 && cmd->has_new_cmd == 1)
			{
				if(cmd->target_mode == CTRL_MODE_MANUAL)
				{
					ctx->cmd_value = cmd->manual_request;
					ctx->app_value = cmd->manual_request;
				}else if(cmd->target_mode == CTRL_MODE_AUTO)
				{
					ctx->current_mode = CTRL_MODE_AUTO;
					ctx->app_value = 0;
					ctx->cmd_value = 0;
				}else if(cmd->target_mode == CTRL_MODE_SAFE)
				{
					ctx->current_mode = CTRL_MODE_SAFE;
				}

				ctx->last_cmd_time = current_time;
				cmd->has_new_cmd = 0;
			}

			if((current_time - ctx->last_cmd_time) >= 2000)
			{
				ctx->current_mode = CTRL_MODE_SAFE;
				ctx->fault_bits |= 0x08;
				ctx->app_value = 0;
				ctx->cmd_value = 0;
				break;
			}

			break;

		case CTRL_MODE_SAFE:
			ctx->cmd_value = 0;
			ctx->app_value = 0;

			if(cmd != 0 && cmd->has_new_cmd && cmd->target_mode == CTRL_MODE_INIT)
			{
				// 核心修改：必须同时满足传感器有效，且距离上次传感器有效时间未超过2000ms（未超时）
				if((sensor->valid_flag == 1) && ((current_time - ctx->last_sensor_time) < 2000))
				{
					ctx->fault_bits = 0;
					ctx->init_start_time = current_time;
					ctx->valid_sample_cnt = 0;
					ctx->current_mode = CTRL_MODE_INIT;
				}

				cmd->has_new_cmd = 0;
			}
			break;
	}
}

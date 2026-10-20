#ifndef __CONTROLCORE_H
#define __CONTROLCORE_H

#include <stdint.h>
#include <stdbool.h>

typedef enum{
	CTRL_MODE_INIT,
	CTRL_MODE_AUTO,
	CTRL_MODE_MANUAL,
	CTRL_MODE_SAFE
}Control_Mode_t;

typedef struct{
	uint32_t timestamp;
	int16_t temperature;
	int16_t humidity;
	uint8_t valid_flag;
	uint32_t error_count;
}Sensor_Sample_t;

typedef struct{
	Control_Mode_t target_mode;
	uint8_t manual_request;
	uint32_t cmd_timestamp;
	uint8_t has_new_cmd;
}ControlCommand_t;

typedef struct{
	Control_Mode_t current_mode;
	uint8_t cmd_value;
	uint8_t app_value;
	uint8_t fault_bits;

	uint32_t init_start_time;
	uint8_t valid_sample_cnt;
	uint32_t last_sensor_time;
	uint32_t last_cmd_time;
}ControlContext_t;

void ControlCore_Init(ControlContext_t *ctx, uint32_t current_time);
void ControlCore_Step(ControlContext_t *ctx, Sensor_Sample_t *sensor, ControlCommand_t *cmd, uint32_t current_time);

#endif

#ifndef __CONTROLCORE_H
#define __CONTROLCORE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define AUTO_ON_TEMP_X10       300     // 30.0°C
#define AUTO_OFF_TEMP_X10      280     // 28.0°C
#define CONTROL_PERCENT_MAX    100U
#define MANUAL_TIMEOUT_MS      2000U
#define SENSOR_TIMEOUT_MS      2000U
#define INIT_TIMEOUT_MS        8000U

typedef enum{
	CTRL_MODE_INIT,
	CTRL_MODE_AUTO,
	CTRL_MODE_MANUAL,
	CTRL_MODE_SAFE
}Control_Mode_t;

typedef enum
{
    CONTROL_CMD_AUTO        = 1,
    CONTROL_CMD_MANUAL      = 2,
    CONTROL_CMD_SAFE        = 3,
    CONTROL_CMD_CLEAR_FAULT = 4
} Control_CommandCode_t;

typedef struct{
	uint32_t timestamp;
	int16_t temperature;
	int16_t humidity;
	uint8_t valid_flag;
	uint32_t error_count;
	uint8_t error_state;
}Sensor_Sample_t;

typedef struct
{
	Control_CommandCode_t command;
    uint8_t target_percent;   // 内部统一为 0~100
    uint8_t rx_counter;       // CAN Rolling Counter
    uint8_t valid;            // 新命令有效标志
    uint32_t timestamp;       // FreeRTOS tick
} ControlCommand_t;

typedef struct{
	Control_Mode_t current_mode;
	uint8_t cmd_value;
	uint8_t app_value;
	uint8_t fault_bits;

	uint32_t init_start_time;
	uint8_t valid_sample_cnt;
	uint32_t last_sensor_time;
	uint32_t last_cmd_time;

	uint8_t relay_state;   // 只允许0或1
}ControlContext_t;

void ControlCore_Init(ControlContext_t *ctx, uint32_t current_time);
void ControlCore_Step(ControlContext_t *ctx, Sensor_Sample_t *sensor, ControlCommand_t *cmd, uint32_t current_time);

#endif

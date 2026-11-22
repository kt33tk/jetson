#ifndef FAN_CONTROL_H
#define FAN_CONTROL_H

#include "FanHardware.h"
#include <stdint.h>

uint8_t FanControl_Init(void);

/*
 * target_percent：控制状态机给出的 0～100 目标值。
 * now_ms：当前毫秒数。
 * 返回 1：要求运转，但转速持续过低。
 */
uint8_t FanControl_Update(uint8_t target_percent, uint32_t now_ms);
uint32_t FanControl_GetRpm(void);

#endif

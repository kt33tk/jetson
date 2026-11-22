#ifndef FAN_HARDWARE_H
#define FAN_HARDWARE_H

#include "main.h"
#include <stdint.h>

/*
 * 硬件接口。
 * 将来换四线风扇时，控制代码继续调用这些函数，
 * 只替换实现它们的硬件文件。
 */
uint8_t FanHw_Init(void);
void FanHw_SetDuty(uint8_t percent);
void FanHw_Stop(void);
uint32_t FanHw_ReadPulses(void);
uint32_t FanHw_PulsesPerRevolution(void);
uint32_t FanHw_MaxTargetRpm(void);

#endif

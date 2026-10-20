#ifndef __CANTX_H
#define __CANTX_H

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "main.h"
#include "Can_protocol.h"
#include <stdlib.h>
#include "sensor.h"
#include "Watchdog.h"
#include "stm32f4xx_hal.h"
#include "Can_protocol.h"
#include "crc8.h"

typedef struct{
	int16_t temperature;
	int16_t humidity;
	uint8_t fan_status;
	uint8_t sys_fault;
}Ui_Snapshot_t;

void Trigger_Send_0x280_ACK(uint8_t rx_counter);
void CanTX_Task(void *pvParameters);

#endif

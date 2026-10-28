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

typedef enum {
    ACK_SUCCESS   = 0x00,  // 正常新命令[cite: 21]
    ACK_ERR_DUP   = 0x01,  // 重复计数器[cite: 21]
    ACK_ERR_CRC   = 0x02,  // CRC校验失败[cite: 21]
    ACK_ERR_PARAM = 0x03   // 非法参数 (如未知的模式值)[cite: 21]
} AckStatus_t;

typedef struct{
	int16_t temperature;
	int16_t humidity;
	uint8_t fan_status;
	uint8_t sys_fault;
}Ui_Snapshot_t;

void Trigger_Send_0x280_ACK(uint8_t rx_counter, uint8_t ack_status);
void CanTX_Task(void *pvParameters);
BaseType_t CanTX_Enqueue(const Can_Frame_t *frame);

#endif

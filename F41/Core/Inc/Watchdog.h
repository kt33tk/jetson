#ifndef __WATCHDOG_H
#define __WATCHDOG_H

//心跳标志位
#define HEARTBEAT_CONTROL  (1 << 0)  // 0x01
#define HEARTBEAT_CANTX    (1 << 1)  // 0x02
#define HEARTBEAT_SENSOR   (1 << 2)  // 0x04

#include "FreeRTOS.h"
#include "main.h"
#include "task.h"
#include "queue.h"
#include <stdio.h>
#include <stdint.h>

void Watchdog_Task(void *pvParameters);

#endif

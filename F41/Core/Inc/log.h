#ifndef __LOG_H
#define __LOG_H

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "CanTX.h"
#include <stdio.h>
#include <stdlib.h>
#include "sensor.h"
#include <string.h>
#include "Can_protocol.h"
#include "crc8.h"

void Log_Task(void *pvParameters);

#endif

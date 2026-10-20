#ifndef __SENSOR_H
#define __SENSOR_H

#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <stdlib.h>
#include "ControlCore.h"
#include "Watchdog.h"

void Sensor_Task(void *pvParameters);

#endif

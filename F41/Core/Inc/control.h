#ifndef __CONTROL_H
#define __CONTROL_H

#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "main.h"
#include "sensor.h"
#include "ControlCore.h"
#include <string.h>
#include "Watchdog.h"

void Control_Task(void *pvParameters);

#endif

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
#include "SystemSync.h"
#include "NVM.h"
#include "Can_protocol.h"
#include "INA226.h"

#define FAULT_OVERVOLTAGE 0x20U
#define FAULT_OVERCURRENT 0x40U
#define FAULT_INA226      0x80U

void Control_Task(void *pvParameters);

#endif

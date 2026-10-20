#ifndef __CANRX_H
#define __CANRX_H

#include "FreeRTOS.h"
#include "main.h"
#include "task.h"
#include "queue.h"
#include "Can_protocol.h"
#include "CanTX.h"

typedef struct {
    uint32_t id;
    uint8_t dlc;
    uint8_t data[8];
    uint32_t timestamp;
} RawCanMsg_t;

void CanRX_Task(void *pvParameters);

#endif

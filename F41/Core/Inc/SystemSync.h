#ifndef __SYSTEM_SYNC_H
#define __SYSTEM_SYNC_H

#define EVT_SENSOR_VALID       (1U << 0)
#define EVT_CAN_READY          (1U << 1)
#define EVT_CONTROL_SAFE       (1U << 2)
#define EVT_NVM_READY          (1U << 3)
#define EVT_UPDATE_REQUESTED   (1U << 4)
#define EVT_UPDATE_ALLOWED     (1U << 5)
#define EVT_FATAL_FAULT        (1U << 6)

#include "FreeRTOS.h"
#include "event_groups.h"
#include "semphr.h"
#include "main.h"
#include <string.h>

extern SemaphoreHandle_t AdcDmaDoneSemaphore;
extern SemaphoreHandle_t CanMailboxSemaphore;
extern SemaphoreHandle_t DiagSnapshotMutex;
extern SemaphoreHandle_t SpiBusMutex;
extern EventGroupHandle_t SystemEventGroup;

typedef struct
{
    int16_t temperature_x10;
    uint8_t sensor_valid;
    uint8_t control_mode;
    uint8_t cmd_value;
    uint8_t app_value;
    uint8_t relay_state;
    uint8_t fault_bits;
    uint32_t can_tx_ok;
    uint32_t can_tx_fail;
    uint32_t can_rx_count;
    uint32_t timestamp;
} DiagSnapshot_t;

BaseType_t DiagSnapshot_UpdateSensor(int16_t temperature_x10, uint8_t valid, uint32_t timestamp);
BaseType_t DiagSnapshot_UpdateControl(uint8_t mode, uint8_t cmd_value, uint8_t app_value, uint8_t relay_state, uint8_t fault_bits, uint32_t timestamp);
BaseType_t DiagSnapshot_Copy(DiagSnapshot_t *output);

BaseType_t SpiBus_Lock(TickType_t timeout);
void SpiBus_Unlock(void);

#endif

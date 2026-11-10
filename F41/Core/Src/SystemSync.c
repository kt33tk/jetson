#include "SystemSync.h"

static DiagSnapshot_t g_diag_snapshot;
volatile uint32_t diag_mutex_timeout_cnt = 0;

BaseType_t DiagSnapshot_UpdateSensor(int16_t temperature_x10, uint8_t valid, uint32_t timestamp)
{
    if (xSemaphoreTake(DiagSnapshotMutex, pdMS_TO_TICKS(5)) != pdTRUE)
    {
        diag_mutex_timeout_cnt++;
        return pdFAIL;
    }

    g_diag_snapshot.temperature_x10 = temperature_x10;
    g_diag_snapshot.sensor_valid = valid;
    g_diag_snapshot.timestamp = timestamp;

    xSemaphoreGive(DiagSnapshotMutex);
    return pdPASS;
}

BaseType_t DiagSnapshot_UpdateControl(uint8_t mode, uint8_t cmd_value, uint8_t app_value, uint8_t relay_state, uint8_t fault_bits, uint32_t timestamp)
{
    if (xSemaphoreTake(DiagSnapshotMutex, pdMS_TO_TICKS(5)) != pdTRUE)
    {
        diag_mutex_timeout_cnt++;
        return pdFAIL;
    }

    g_diag_snapshot.control_mode = mode;
    g_diag_snapshot.cmd_value = cmd_value;
    g_diag_snapshot.app_value = app_value;
    g_diag_snapshot.relay_state = relay_state;
    g_diag_snapshot.fault_bits = fault_bits;
    g_diag_snapshot.timestamp = timestamp;

    xSemaphoreGive(DiagSnapshotMutex);
    return pdPASS;
}

BaseType_t DiagSnapshot_Copy(DiagSnapshot_t *output)
{
    if (output == NULL)
    {
        return pdFAIL;
    }

    if (xSemaphoreTake(DiagSnapshotMutex, pdMS_TO_TICKS(5)) != pdTRUE)
    {
        diag_mutex_timeout_cnt++;
        return pdFAIL;
    }

    memcpy(output, &g_diag_snapshot, sizeof(*output));

    xSemaphoreGive(DiagSnapshotMutex);
    return pdPASS;
}

BaseType_t SpiBus_Lock(TickType_t timeout)
{
    if (SpiBusMutex == NULL)
    {
        return pdFAIL;
    }

    return xSemaphoreTake(SpiBusMutex, timeout);
}

void SpiBus_Unlock(void)
{
    if (SpiBusMutex != NULL)
    {
        xSemaphoreGive(SpiBusMutex);
    }
}

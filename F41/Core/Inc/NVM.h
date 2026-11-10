#ifndef __NVM_H
#define __NVM_H

#include "FreeRTOS.h"
#include "queue.h"
#include "NvM_Map.h"
#include "CRC32.h"
#include "W25Q64.h"
#include "SystemSync.h"
#include "task.h"
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define NVM_PARAM_MAGIC          0x5041524DUL  /* PARM */
#define NVM_PARAM_VERSION        1U
#define NVM_PARAM_COMMIT_MARKER  0x434F4D54UL  /* COMT */

#define NVM_DTC_MAGIC       0xD7C1U
#define NVM_DTC_VERSION     1U

#define NVM_DTC_RECORD_SIZE         32UL
#define NVM_DTC_RECORDS_PER_SECTOR  \
    (W25Q64_SECTOR_SIZE / NVM_DTC_RECORD_SIZE)

#define NVM_DTC_TOTAL_RECORDS       \
    (NVM_DTC_RECORDS_PER_SECTOR * \
     NVM_DTC_SECTOR_COUNT)

#define NVM_QUEUE_LENGTH 8U

_Static_assert(NVM_DTC_RECORDS_PER_SECTOR == 128UL, "Unexpected DTC records per sector");
_Static_assert(NVM_DTC_TOTAL_RECORDS == 512UL, "Unexpected total DTC records");

//业务参数
typedef struct
{
    int16_t auto_on_temp_x10;
    int16_t auto_off_temp_x10;

    uint32_t manual_timeout_ms;
    uint32_t sensor_timeout_ms;

    uint8_t reserved[16];
} NvM_ParamPayload_t;

//写入的参数记录
typedef struct
{
    uint32_t magic;
    uint16_t format_version;
    uint16_t payload_length;
    uint32_t sequence;

    NvM_ParamPayload_t payload;

    uint32_t crc32;
    uint32_t commit_marker;
} NvM_ParamRecord_t;

//故障变化
typedef enum
{
    NVM_DTC_EVENT_SET = 1,
    NVM_DTC_EVENT_CLEAR = 2
} NvM_DtcEventType_t;

//FLASH的DTC记录
typedef struct
{
    uint16_t magic;
    uint8_t version;
    uint8_t event;

    uint32_t sequence;
    uint32_t timestamp;

    uint16_t dtc_code;
    uint8_t dtc_status;
    uint8_t control_mode;

    int16_t temperature_x10;
    uint8_t app_value;
    uint8_t relay_state;

    uint32_t can_error;
    uint32_t reserved;

    uint32_t crc32;
} NvM_DtcRecord_t;

//提交的DTC快照
typedef struct
{
    uint8_t event;
    uint16_t dtc_code;
    uint8_t dtc_status;
    uint8_t control_mode;

    int16_t temperature_x10;
    uint8_t app_value;
    uint8_t relay_state;

    uint32_t timestamp;
    uint32_t can_error;
} NvM_DtcRequestData_t;

typedef enum
{
    NVM_REQUEST_SAVE_PARAMS = 1,
    NVM_REQUEST_APPEND_DTC,
    NVM_REQUEST_CLEAR_DTC
} NvM_RequestType_t;

//NVM队列消息
typedef struct
{
    NvM_RequestType_t type;
    TaskHandle_t reply_task;

    union
    {
        NvM_ParamPayload_t params;
        NvM_DtcRequestData_t dtc;
    } data;
} NvM_Request_t;

//DTC槽位状态
typedef enum
{
    NVM_DTC_SLOT_READ_ERROR = 0,
    NVM_DTC_SLOT_ERASED,
    NVM_DTC_SLOT_OCCUPIED
} NvM_DtcSlotState_t;

_Static_assert(sizeof(NvM_ParamPayload_t) == 28U, "Unexpected parameter payload size");
_Static_assert(sizeof(NvM_ParamRecord_t) == 48U, "Unexpected parameter record size");
_Static_assert(sizeof(NvM_DtcRecord_t) == 32U, "DTC record must be exactly 32 bytes");

extern const NvM_ParamPayload_t NvM_DefaultParams;

uint8_t NvM_BootInit(NvM_ParamPayload_t *loaded_params);
uint8_t NvM_RequestSaveParams(const NvM_ParamPayload_t *params);
void NvM_Task(void *pvParameters);
uint8_t NvM_RequestDtc(const NvM_DtcRequestData_t *dtc);
uint8_t NvM_ReadLatestSetDtc(uint16_t dtc_code, NvM_DtcRecord_t *output);
uint8_t NvM_ClearDtcHistory(void);

#endif

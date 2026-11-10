#ifndef __W25Q64_H
#define __W25Q64_H

#include "main.h"
#include "SystemSync.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdint.h>

#define W25Q64_CMD_JEDEC_ID        0x9FU
#define W25Q64_CMD_READ_DATA       0x03U
#define W25Q64_CMD_PAGE_PROGRAM    0x02U
#define W25Q64_CMD_SECTOR_ERASE    0x20U
#define W25Q64_CMD_WRITE_ENABLE    0x06U
#define W25Q64_CMD_READ_STATUS1    0x05U

#define W25Q64_PAGE_SIZE           256U
#define W25Q64_SECTOR_SIZE         4096U
#define W25Q64_TOTAL_SIZE          (8U * 1024U * 1024U)

#define W25Q64_SPI_TIMEOUT_MS      50U
#define W25Q64_PROGRAM_TIMEOUT_MS  20U
#define W25Q64_ERASE_TIMEOUT_MS    1000U
#define W25Q64_STATUS_BUSY  0x01U
#define W25Q64_READ_CHUNK_SIZE 4096U

typedef enum
{
    W25Q64_OK = 0,
    W25Q64_ERROR,
    W25Q64_TIMEOUT,
    W25Q64_INVALID_PARAM,
    W25Q64_ID_ERROR
} W25Q64_Status_t;

typedef struct
{
    volatile uint32_t lock_timeout_cnt;
    volatile uint32_t spi_tx_error_cnt;
    volatile uint32_t spi_rx_error_cnt;
    volatile uint32_t spi_timeout_cnt;
    volatile uint32_t busy_timeout_cnt;
    volatile uint32_t id_error_cnt;
    volatile uint32_t write_enable_error_cnt;
} W25Q64_ErrorStats_t;

extern volatile W25Q64_ErrorStats_t w25q64_error_stats;

W25Q64_Status_t W25Q64_Init(void);
W25Q64_Status_t W25Q64_ReadJedecId(uint8_t id[3]);
W25Q64_Status_t W25Q64_Read(uint32_t address, uint8_t *data, uint32_t length);
W25Q64_Status_t W25Q64_PageProgram(uint32_t address, const uint8_t *data, uint16_t length);
W25Q64_Status_t W25Q64_Write(uint32_t address, const uint8_t *data, uint32_t length);
W25Q64_Status_t W25Q64_EraseSector4K(uint32_t address);
W25Q64_Status_t W25Q64_ReadStatus1(uint8_t *status);

#endif

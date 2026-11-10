#include "W25Q64.h"

extern SPI_HandleTypeDef hspi1;

volatile W25Q64_ErrorStats_t w25q64_error_stats = {0};

#define W25Q64_STATUS_BUSY  0x01U
#define W25Q64_STATUS_WEL   0x02U

static void W25Q64_CS_Low(void)
{
    HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_RESET);
}

static void W25Q64_CS_High(void)
{
    HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_SET);
}

static W25Q64_Status_t W25Q64_ConvertHalStatus(HAL_StatusTypeDef hal_status, uint8_t is_transmit)
{
    if (hal_status == HAL_OK)
    {
        return W25Q64_OK;
    }

    if (is_transmit != 0U)
    {
        w25q64_error_stats.spi_tx_error_cnt++;
    }
    else
    {
        w25q64_error_stats.spi_rx_error_cnt++;
    }

    if (hal_status == HAL_TIMEOUT)
    {
        w25q64_error_stats.spi_timeout_cnt++;
        return W25Q64_TIMEOUT;
    }

    return W25Q64_ERROR;
}

static W25Q64_Status_t W25Q64_ReadJedecId_Locked(uint8_t id[3])
{
    uint8_t command = W25Q64_CMD_JEDEC_ID;
    W25Q64_Status_t result = W25Q64_OK;

    W25Q64_CS_Low();

    result = W25Q64_ConvertHalStatus(HAL_SPI_Transmit(&hspi1, &command, 1, W25Q64_SPI_TIMEOUT_MS), 1U);

    if (result != W25Q64_OK)
    {
        goto exit;
    }

    result = W25Q64_ConvertHalStatus(HAL_SPI_Receive(&hspi1, id, 3,  W25Q64_SPI_TIMEOUT_MS),0U);

exit:
    W25Q64_CS_High();
    return result;
}

W25Q64_Status_t W25Q64_ReadJedecId(uint8_t id[3])
{
    W25Q64_Status_t result;

    if (id == NULL)
    {
        return W25Q64_INVALID_PARAM;
    }

    if (SpiBus_Lock(pdMS_TO_TICKS(50)) != pdTRUE)
    {
        return W25Q64_TIMEOUT;
    }

    result = W25Q64_ReadJedecId_Locked(id);

    SpiBus_Unlock();

    return result;
}

W25Q64_Status_t W25Q64_Init(void)
{
    uint8_t id[3];
    W25Q64_Status_t result;

    result = W25Q64_ReadJedecId(id);

    if (result != W25Q64_OK)
    {
        return result;
    }

    if ((id[0] != 0xEFU) || (id[1] != 0x40U) || (id[2] != 0x17U))
    {
    	w25q64_error_stats.id_error_cnt++;

        return W25Q64_ID_ERROR;
    }

    return W25Q64_OK;
}

static W25Q64_Status_t W25Q64_ReadStatus1_Locked(uint8_t *status)
{
    uint8_t command = W25Q64_CMD_READ_STATUS1;
    W25Q64_Status_t result = W25Q64_OK;

    if (status == NULL)
    {
        return W25Q64_INVALID_PARAM;
    }

    W25Q64_CS_Low();

    result = W25Q64_ConvertHalStatus(HAL_SPI_Transmit(&hspi1, &command, 1, W25Q64_SPI_TIMEOUT_MS), 1U);

    if (result != W25Q64_OK)
    {
        goto exit;
    }

    result = W25Q64_ConvertHalStatus(HAL_SPI_Receive(&hspi1, status, 1,  W25Q64_SPI_TIMEOUT_MS),0U);

exit:
    W25Q64_CS_High();
    return result;
}

W25Q64_Status_t W25Q64_ReadStatus1(uint8_t *status)
{
    W25Q64_Status_t result;

    if (status == NULL)
    {
        return W25Q64_INVALID_PARAM;
    }

    if (SpiBus_Lock(pdMS_TO_TICKS(50)) != pdTRUE)
    {
        return W25Q64_TIMEOUT;
    }

    result = W25Q64_ReadStatus1_Locked(status);

    SpiBus_Unlock();

    return result;
}

static W25Q64_Status_t W25Q64_WaitBusy_Locked(uint32_t timeout_ms)
{
    uint32_t start_tick;
    uint8_t status;
    W25Q64_Status_t result;

    start_tick = HAL_GetTick();

    for (;;)
    {
        result = W25Q64_ReadStatus1_Locked(&status);

        if (result != W25Q64_OK)
        {
            return result;
        }

        if ((status & W25Q64_STATUS_BUSY) == 0U)
        {
            return W25Q64_OK;
        }

        if ((HAL_GetTick() - start_tick) >= timeout_ms)
        {
        	w25q64_error_stats.busy_timeout_cnt++;

            return W25Q64_TIMEOUT;
        }

        if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
        {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
        else
        {
            HAL_Delay(1);
        }
    }
}

static W25Q64_Status_t W25Q64_WriteEnable_Locked(void)
{
    uint8_t command = W25Q64_CMD_WRITE_ENABLE;
    uint8_t status = 0;
    W25Q64_Status_t result;

    W25Q64_CS_Low();

    result = W25Q64_ConvertHalStatus(HAL_SPI_Transmit(&hspi1, &command, 1, W25Q64_SPI_TIMEOUT_MS), 1U);

    W25Q64_CS_High();

    if (result != W25Q64_OK)
    {
        return result;
    }

    result = W25Q64_ReadStatus1_Locked(&status);

    if (result != W25Q64_OK)
    {
        return result;
    }

    if ((status & W25Q64_STATUS_WEL) == 0U)
    {
        w25q64_error_stats.write_enable_error_cnt++;
        return W25Q64_ERROR;
    }

    return W25Q64_OK;
}

static W25Q64_Status_t W25Q64_Read_Locked(uint32_t address, uint8_t *data, uint32_t length)
{
    uint8_t header[4];
    uint32_t remaining = length;
    uint16_t chunk;
    W25Q64_Status_t result;

    header[0] = W25Q64_CMD_READ_DATA;
    header[1] = (uint8_t)(address >> 16);
    header[2] = (uint8_t)(address >> 8);
    header[3] = (uint8_t)address;

    W25Q64_CS_Low();

    result = W25Q64_ConvertHalStatus(HAL_SPI_Transmit(&hspi1, header, sizeof(header), W25Q64_SPI_TIMEOUT_MS), 1U);

    if (result != W25Q64_OK)
    {
        goto exit;
    }

    while (remaining > 0U)
    {
    	chunk = (remaining > W25Q64_READ_CHUNK_SIZE) ? W25Q64_READ_CHUNK_SIZE : (uint16_t)remaining;

        result = W25Q64_ConvertHalStatus(HAL_SPI_Receive(&hspi1, data, chunk, W25Q64_SPI_TIMEOUT_MS), 0U);

        if (result != W25Q64_OK)
        {
            goto exit;
        }

        data += chunk;
        remaining -= chunk;
    }

exit:
    W25Q64_CS_High();
    return result;
}

W25Q64_Status_t W25Q64_Read(uint32_t address, uint8_t *data, uint32_t length)
{
    W25Q64_Status_t result;

    if ((data == NULL) || (length == 0U))
    {
        return W25Q64_INVALID_PARAM;
    }

    if ((address >= W25Q64_TOTAL_SIZE) || (length > (W25Q64_TOTAL_SIZE - address)))
    {
        return W25Q64_INVALID_PARAM;
    }

    if (SpiBus_Lock(pdMS_TO_TICKS(50)) != pdTRUE)
    {
        w25q64_error_stats.lock_timeout_cnt++;
        return W25Q64_TIMEOUT;
    }

    result = W25Q64_Read_Locked(address, data, length);

    SpiBus_Unlock();
    return result;
}

static W25Q64_Status_t W25Q64_PageProgram_Locked(uint32_t address, const uint8_t *data, uint16_t length)
{
    uint8_t header[4];
    W25Q64_Status_t result;

    result = W25Q64_WaitBusy_Locked(W25Q64_ERASE_TIMEOUT_MS);

    if (result != W25Q64_OK)
    {
        return result;
    }

    result = W25Q64_WriteEnable_Locked();

    if (result != W25Q64_OK)
    {
        return result;
    }

    header[0] = W25Q64_CMD_PAGE_PROGRAM;
    header[1] = (uint8_t)(address >> 16);
    header[2] = (uint8_t)(address >> 8);
    header[3] = (uint8_t)address;

    W25Q64_CS_Low();

    result = W25Q64_ConvertHalStatus(HAL_SPI_Transmit(&hspi1, header, sizeof(header), W25Q64_SPI_TIMEOUT_MS), 1U);

    if (result != W25Q64_OK)
    {
        goto exit;
    }

    result = W25Q64_ConvertHalStatus(HAL_SPI_Transmit(&hspi1, (uint8_t *)data, length, W25Q64_SPI_TIMEOUT_MS),1U);

exit:
    W25Q64_CS_High();

    if (result != W25Q64_OK)
    {
        return result;
    }

    return W25Q64_WaitBusy_Locked(W25Q64_PROGRAM_TIMEOUT_MS);
}

W25Q64_Status_t W25Q64_PageProgram(uint32_t address, const uint8_t *data, uint16_t length)
{
    W25Q64_Status_t result;
    uint32_t page_remaining;

    if ((data == NULL) || (length == 0U) || (length > W25Q64_PAGE_SIZE))
    {
        return W25Q64_INVALID_PARAM;
    }

    if ((address >= W25Q64_TOTAL_SIZE) || (length > (W25Q64_TOTAL_SIZE - address)))
    {
        return W25Q64_INVALID_PARAM;
    }

    page_remaining = W25Q64_PAGE_SIZE - (address & (W25Q64_PAGE_SIZE - 1U));

    if (length > page_remaining)
    {
        return W25Q64_INVALID_PARAM;
    }

    if (SpiBus_Lock(pdMS_TO_TICKS(50)) != pdTRUE)
    {
        w25q64_error_stats.lock_timeout_cnt++;
        return W25Q64_TIMEOUT;
    }

    result = W25Q64_PageProgram_Locked(address, data, length);

    SpiBus_Unlock();
    return result;
}

W25Q64_Status_t W25Q64_Write(uint32_t address, const uint8_t *data, uint32_t length)
{
    uint32_t page_remaining;
    uint16_t chunk;
    W25Q64_Status_t result = W25Q64_OK;

    if ((data == NULL) || (length == 0U))
    {
        return W25Q64_INVALID_PARAM;
    }

    if ((address >= W25Q64_TOTAL_SIZE) || (length > (W25Q64_TOTAL_SIZE - address)))
    {
        return W25Q64_INVALID_PARAM;
    }

    if (SpiBus_Lock(pdMS_TO_TICKS(50)) != pdTRUE)
    {
        w25q64_error_stats.lock_timeout_cnt++;
        return W25Q64_TIMEOUT;
    }

    while (length > 0U)
    {
        page_remaining =W25Q64_PAGE_SIZE - (address & (W25Q64_PAGE_SIZE - 1U));

        chunk = (length < page_remaining) ? (uint16_t)length : (uint16_t)page_remaining;

        result = W25Q64_PageProgram_Locked(address, data, chunk);

        if (result != W25Q64_OK)
        {
            break;
        }

        address += chunk;
        data += chunk;
        length -= chunk;
    }

    SpiBus_Unlock();
    return result;
}

W25Q64_Status_t W25Q64_EraseSector4K(uint32_t address)
{
    uint8_t command[4];
    W25Q64_Status_t result;

    if (address >= W25Q64_TOTAL_SIZE)
    {
        return W25Q64_INVALID_PARAM;
    }

    if ((address % W25Q64_SECTOR_SIZE) != 0U)
    {
        return W25Q64_INVALID_PARAM;
    }

    if (SpiBus_Lock(pdMS_TO_TICKS(50)) != pdTRUE)
    {
        w25q64_error_stats.lock_timeout_cnt++;
        return W25Q64_TIMEOUT;
    }

    result = W25Q64_WaitBusy_Locked(W25Q64_ERASE_TIMEOUT_MS);

    if (result != W25Q64_OK)
    {
        goto unlock;
    }

    result = W25Q64_WriteEnable_Locked();

    if (result != W25Q64_OK)
    {
        goto unlock;
    }

    command[0] = W25Q64_CMD_SECTOR_ERASE;
    command[1] = (uint8_t)(address >> 16);
    command[2] = (uint8_t)(address >> 8);
    command[3] = (uint8_t)address;

    W25Q64_CS_Low();

    result = W25Q64_ConvertHalStatus(HAL_SPI_Transmit( &hspi1, command, sizeof(command), W25Q64_SPI_TIMEOUT_MS), 1U);

    W25Q64_CS_High();

    if (result == W25Q64_OK)
    {
        result = W25Q64_WaitBusy_Locked(W25Q64_ERASE_TIMEOUT_MS);
    }

unlock:
    SpiBus_Unlock();
    return result;
}

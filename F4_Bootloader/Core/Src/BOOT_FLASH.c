#include "BOOT_FLASH.h"

extern SPI_HandleTypeDef hspi1;

//CS
static void Select(uint8_t active)
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, active ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

static uint8_t WaitReady(uint32_t timeout)
{
    uint8_t command = 0x05U;         //读取状态
    uint8_t status;
    uint32_t start = HAL_GetTick();

    do
    {
        Select(1U);

        HAL_StatusTypeDef result = HAL_SPI_Transmit(&hspi1, &command, 1U, 50U);

        if (result == HAL_OK)
            result = HAL_SPI_Receive(&hspi1, &status, 1U, 50U);

        Select(0U);

        if (result != HAL_OK)
            return 0U;

        if ((status & 1U) == 0U)
            return 1U;

    } while (HAL_GetTick() - start < timeout);

    return 0U;
}

static uint8_t WriteEnable(void)
{
    uint8_t command = 0x06U;         //写使能

    Select(1U);
    HAL_StatusTypeDef result = HAL_SPI_Transmit(&hspi1, &command, 1U, 50U);
    Select(0U);

    return result == HAL_OK;
}

uint8_t Bf_Init(void)
{
    uint8_t command = 0x9FU;         //读ID
    uint8_t id[3];

    Select(0U);

    /* MCU复位时，外部Flash可能仍在完成上一次擦写。 */
    if (!WaitReady(2000U))
        return 0U;

    Select(1U);

    HAL_StatusTypeDef result = HAL_SPI_Transmit(&hspi1, &command, 1U, 50U);

    if (result == HAL_OK)
        result = HAL_SPI_Receive(&hspi1, id, 3U, 50U);

    Select(0U);

    return result == HAL_OK &&
           id[0] == 0xEFU &&
           id[1] == 0x40U &&
           id[2] == 0x17U;
}

uint8_t Bf_Read(uint32_t address, void *data, uint32_t length)
{
    uint8_t command[4] = {0x03U, (uint8_t)(address >> 16), (uint8_t)(address >> 8), (uint8_t)address};         //指定地址连续读取

    uint8_t *output = data;
    HAL_StatusTypeDef result;

    Select(1U);

    result = HAL_SPI_Transmit(&hspi1, command, 4U, 50U);

    while (result == HAL_OK && length != 0U)
    {
        uint16_t count = length > 4096U ? 4096U : (uint16_t)length;

        result = HAL_SPI_Receive(&hspi1, output, count, 50U);
        output += count;
        length -= count;
    }

    Select(0U);
    return result == HAL_OK;
}

uint8_t Bf_Write(uint32_t address, const void *data, uint32_t length)
{
    const uint8_t *input = data;

    while (length != 0U)
    {
        uint16_t count = (uint16_t)(256U - (address & 255U));

        if (count > length)
            count = (uint16_t)length;

        if (!WaitReady(1000U) || !WriteEnable())
            return 0U;

        uint8_t command[4] = {0x02U, (uint8_t)(address >> 16), (uint8_t)(address >> 8), (uint8_t)address};        //页编程

        Select(1U);

        HAL_StatusTypeDef result = HAL_SPI_Transmit(&hspi1, command, 4U, 50U);

        if (result == HAL_OK)
        {
            result = HAL_SPI_Transmit(&hspi1, (uint8_t *)input, count, 50U);
        }

        Select(0U);

        if (result != HAL_OK || !WaitReady(20U))
            return 0U;

        address += count;
        input += count;
        length -= count;
    }

    return 1U;
}

uint8_t Bf_Erase(uint32_t address)
{
    uint8_t command[4] = {0x20U, (uint8_t)(address >> 16), (uint8_t)(address >> 8), (uint8_t)address};             //擦除

    if (!WaitReady(1000U) || !WriteEnable())
        return 0U;

    Select(1U);

    HAL_StatusTypeDef result = HAL_SPI_Transmit(&hspi1, command, 4U, 50U);

    Select(0U);

    return result == HAL_OK && WaitReady(1000U);
}

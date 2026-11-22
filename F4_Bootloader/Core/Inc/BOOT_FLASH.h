#ifndef BOOT_FLASH_H
#define BOOT_FLASH_H

#include "main.h"
#include <stdint.h>

uint8_t Bf_Init(void);
uint8_t Bf_Read(uint32_t address, void *data, uint32_t length);
uint8_t Bf_Write(uint32_t address, const void *data, uint32_t length);
uint8_t Bf_Erase(uint32_t address);

#endif

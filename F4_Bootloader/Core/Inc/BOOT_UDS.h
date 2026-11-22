#ifndef __BOOT_UDS_H
#define __BOOT_UDS_H

#include "BOOT_ISOTP.h"
#include "BOOT_IMAGE.h"
#include "ECU_INFO.h"
#include <string.h>

void BootUds_Init(uint8_t flash_ready);
void BootUds_OnMessage(const uint8_t *request, uint16_t length);
void BootUds_Poll(CAN_HandleTypeDef *can);

#endif

#ifndef BOOT_ISOTP_H
#define BOOT_ISOTP_H

#include "boot_meta.h"
#include <string.h>
#include <stdint.h>
#include "stm32f4xx_hal.h"

#define BL_ISOTP_RX_ID  0x7E0U
#define BL_ISOTP_TX_ID  0x7E8U
#define BL_ISOTP_MAX    256U
#define TP_TIMEOUT_MS  1000U
#define RX_BLOCK_SIZE  4U
#define RX_STMIN_MS     5U

typedef void (*BlIsoTpMessageFn)(const uint8_t *data, uint16_t length);
void BlIsoTp_Init(CAN_HandleTypeDef *can, BlIsoTpMessageFn on_message);
void BlIsoTp_OnFrame(const CAN_RxHeaderTypeDef *header, const uint8_t data[8]);
uint8_t BlIsoTp_Send(const uint8_t *data, uint16_t length);
void BlIsoTp_Poll(void);

#endif

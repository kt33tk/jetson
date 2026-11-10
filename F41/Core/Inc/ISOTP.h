#ifndef __ISOTP_H
#define __ISOTP_H

#include "Can_protocol.h"
#include "CanTX.h"
#include <string.h>
#include <stdint.h>

#define ISOTP_REQUEST_ID   0x7E0U
#define ISOTP_RESPONSE_ID  0x7E8U
#define ISOTP_MAX_PAYLOAD  48U

typedef void (*IsoTp_RxCallback_t)(const uint8_t *data, uint16_t length);

void IsoTp_Init(IsoTp_RxCallback_t callback);
void IsoTp_OnCanFrame(const Can_Frame_t *frame);
uint8_t IsoTp_Send(const uint8_t *data, uint16_t length);

#endif

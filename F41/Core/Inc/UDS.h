#ifndef __UDS_H
#define __UDS_H

#include "ISOTP.h"
#include "NVM.h"
#include "SystemSync.h"
#include "Can_protocol.h"
#include <stdint.h>
#include "ECU_INFO.h"

//版本号
#define ECU_SW_MAJOR  APP_SW_MAJOR
#define ECU_SW_MINOR  APP_SW_MINOR
#define ECU_SW_PATCH  APP_SW_PATCH

//诊断请求失败原因
#define NRC_SERVICE_NOT_SUPPORTED       0x11U     //SID不认识
#define NRC_SUBFUNCTION_NOT_SUPPORTED   0x12U     //子功能不支持
#define NRC_INCORRECT_LENGTH            0x13U     //请求长度、格式不正确
#define NRC_CONDITIONS_NOT_CORRECT      0x22U     //运行状态不支持
#define NRC_REQUEST_OUT_OF_RANGE        0x31U     //DID、DTC不认识

void Uds_HandleRequest(const uint8_t *request, uint16_t length);

#endif

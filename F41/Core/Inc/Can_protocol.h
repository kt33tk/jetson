#ifndef __CAN_PROTOCOL_H
#define __CAN_PROTOCOL_H

#include <stdint.h>
#include "crc8.h"
#include "ControlCore.h"

#define CAN_TARGET_RAW_MAX 1000U

extern volatile uint32_t can_tx_success_cnt;
extern volatile uint32_t can_tx_fail_cnt;
extern volatile uint32_t can_rx_cnt;
extern volatile uint32_t can_busoff_cnt;

extern volatile uint32_t can_ack_error_cnt;
extern volatile uint32_t can_bit_error_cnt;
extern volatile uint32_t can_last_error;

extern volatile uint8_t can_last_tec;
extern volatile uint8_t can_last_rec;

extern volatile uint32_t can_rx_queue_full_cnt;
extern volatile uint32_t can_tx_queue_full_cnt;
extern volatile uint32_t can_ack_queue_fail_cnt;

typedef struct {
    int16_t temperature;
    int16_t humidity;
    uint8_t valid_flag;
    uint8_t app_value;
    uint8_t current_mode;
    uint8_t fault_bits;
    uint8_t rx_counter; // 接收时提取出的对方 counter
} SensorMsg_t;

typedef struct{
	uint32_t id;
	uint8_t dlc;
	uint8_t data[8];
}Can_Frame_t;

typedef enum {
    RX_VALID_NEW = 0,   // 全新且合法的指令
    RX_DUPLICATE,       // 重复指令（需回复 0x280）
    RX_ERR_ID,          // ID 错误
    RX_ERR_DLC,         // 长度错误
    RX_ERR_CRC,         // 校验码错误
	RX_ERR_PARAM        // 参数错误
} Rx_Filter_Status_t;

typedef struct {
    uint8_t echoed_id;      // 产生重复冲突的原始目标 ID (如 0x20)
    uint8_t duplicate_cnt;  // 被拦截的重复 Counter 值
    uint8_t status_code;    // 状态码 (如 0x01 代表 Counter 相同拦截)
} AckMsg_t;

void Pack_Msg_0x180(SensorMsg_t *data, uint8_t *payload);
void Unpack_Msg_0x180(uint8_t *payload, SensorMsg_t *data);
void Pack_Msg_0x200(ControlCommand_t *cmd, uint8_t *payload);
void Unpack_Msg_0x200(uint8_t *payload, ControlCommand_t *cmd);
Rx_Filter_Status_t Filter_And_Decode_0x200(Can_Frame_t *rx_frame, ControlCommand_t *out_cmd);
void Unpack_Msg_0x181(uint8_t *payload, SensorMsg_t *data);
void Unpack_Msg_0x280(uint8_t *payload, AckMsg_t *data);
void Commit_0x200_Counter(uint8_t counter);

#endif

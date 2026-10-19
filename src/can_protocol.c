#include "can_protocol.h"

void Pack_Msg_0x180(SensorMsg_t *data, uint8_t *payload)
{
    payload[0] = (data->temperature >> 8) & 0xFF;
    payload[1] = data->temperature & 0xFF;
    payload[2] = (data->humidity >> 8) & 0xFF;
    payload[3] = data->humidity & 0xFF;

    // Byte 4: 最高位(bit 7)放 valid_flag，低7位放 fault_bits
    payload[4] = ((data->valid_flag & 0x01) << 7) | (data->fault_bits & 0x7F);

    payload[5] = data->app_value;

    static uint8_t counter_180 = 0;
    counter_180 = (counter_180 + 1) & 0x0F;

    payload[6] = ((data->current_mode & 0x0F) << 4) | counter_180;

    // 强制霸占 Byte 7 作为安全底线
    payload[7] = Calculate_CRC8_J1850(payload, 7);
}

void Unpack_Msg_0x180(uint8_t *payload, SensorMsg_t *data)
{
    // 1. 组合 8 位数据恢复为 16 位 (高位左移 8 位后，与低位按位或)
    data->temperature = (payload[0] << 8) | payload[1];
    data->humidity    = (payload[2] << 8) | payload[3];

    // 2. 提取普通 8 位状态
    data->valid_flag = (payload[4] >> 7) & 0x01; // 右移 7 位将其归位，提取纯粹的 0 或 1
    data->fault_bits = payload[4] & 0x7F;        // 屏蔽最高位，提取低 7 位的故障码

    data->app_value  = payload[5];

    // 3. 剥离 Byte 6 的复用数据
    data->current_mode = (payload[6] >> 4) & 0x0F; // 提取高 4 位
    data->rx_counter   = payload[6] & 0x0F;        // 提取低 4 位 (供过滤漏斗核对)
}

void Pack_Msg_0x200(ControlCmd_t *cmd, uint8_t *payload)
{
    payload[0] = cmd->target_mode;

    payload[1] = (cmd->target_value >> 8) & 0xFF;
    payload[2] = cmd->target_value & 0xFF;

    payload[3] = 0x00; // 保留空闲字节
    payload[4] = 0x00;
    payload[5] = 0x00;

    // Rolling Counter
    static uint8_t counter_200 = 0;
    counter_200 = (counter_200 + 1) & 0x0F;
    payload[6] = counter_200;

    // CRC
    payload[7] = Calculate_CRC8_J1850(payload, 7);
}

void Unpack_Msg_0x200(uint8_t *payload, ControlCmd_t *cmd)
{
    cmd->target_mode  = payload[0];
    cmd->target_value = (payload[1] << 8) | payload[2];
    cmd->rx_counter   = payload[6] & 0x0F;
}

Rx_Filter_Status_t Filter_And_Decode_0x200(Can_Frame_t *rx_frame, ControlCmd_t *out_cmd)
{
    // 1. 检查 ID
    if (rx_frame->id != 0x200) {
        return RX_ERR_ID;
    }

    // 2. 检查数据长度 (DLC)[cite: 16]
    if (rx_frame->dlc != 8) {
        return RX_ERR_DLC;
    }

    // 3. 计算并对比 CRC[cite: 16]
    // 取前 7 个字节算 CRC，和第 8 个字节 (data[7]) 对比
    if (Calculate_CRC8_J1850(rx_frame->data, 7) != rx_frame->data[7]) {
        return RX_ERR_CRC; // CRC 错误，说明总线受到严重干扰，直接丢弃
    }

    // 4. 核对 Counter[cite: 16]
    uint8_t current_counter = rx_frame->data[6] & 0x0F;
    static uint8_t last_valid_counter = 0xFF; // 初始设为 0xFF(无效值)，确保第一帧能通过

    if (current_counter == last_valid_counter) {
        // 如果和上次的 counter 一模一样，说明是发送方没收到应答触发的重发[cite: 16]
        return RX_DUPLICATE;
    }

    // 5. 全部通过，转换为应用层的 ControlCmd_t 结构体[cite: 16]
    // 这里调用你之前写好的 Unpack 函数
    Unpack_Msg_0x200(rx_frame->data, out_cmd);

    // 更新历史 counter
    last_valid_counter = current_counter;

    return RX_VALID_NEW;
}

void Unpack_Msg_0x181(uint8_t *payload, SensorMsg_t *data)
{
    Unpack_Msg_0x180(payload, data);
}

// 0x280 解包：提取应答状态
void Unpack_Msg_0x280(uint8_t *payload, AckMsg_t *data)
{
    data->echoed_id     = payload[0]; // Byte 0 放原始 ID
    data->duplicate_cnt = payload[1]; // Byte 1 放重复的 Counter
    data->status_code   = payload[2]; // Byte 2 放状态码
}

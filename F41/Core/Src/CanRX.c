#include "CanRX.h"

extern QueueHandle_t Queue_CAN_RX_t;
extern QueueHandle_t Queue_Control_Cmd_t;

extern volatile uint32_t can_rx_cnt;
extern volatile uint32_t can_busoff_cnt;
extern volatile uint32_t can_tx_fail_cnt;

void CanRX_Task(void *pvParameters)
{
    Can_Frame_t raw_rx_msg;
    ControlCmd_t valid_cmd;

    for(;;) {
        if (xQueueReceive(Queue_CAN_RX_t, &raw_rx_msg, portMAX_DELAY) == pdTRUE) {

            if (raw_rx_msg.id == 0x200) {
                // 将原始报文扔进漏斗过滤
                Rx_Filter_Status_t status = Filter_And_Decode_0x200(&raw_rx_msg, &valid_cmd);

                if (status == RX_VALID_NEW) {
                    // 全新指令：推送到业务控制队列，触发实际动作[cite: 16]
                    xQueueSend(Queue_Control_Cmd_t, &valid_cmd, 0);

                    // 正常回复 0x280 应答帧
                    Trigger_Send_0x280_ACK(valid_cmd.rx_counter);
                }
                else if (status == RX_DUPLICATE) {
                    // 重复指令：不执行实际动作（不进控制队列），但必须触发发送 0x280[cite: 16]
                    Trigger_Send_0x280_ACK(raw_rx_msg.data[6] & 0x0F);
                }
                // 如果是 RX_ERR_CRC 或 RX_ERR_DLC，直接无视并丢弃，什么都不做
            }
        }
    }
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CAN_RxHeaderTypeDef RxHeader;
    uint8_t RxData[8];
    Can_Frame_t raw_msg; // 使用你定义的 CAN 帧结构体
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    // 必须把数据读出来，否则会一直卡在中断里
    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &RxHeader, RxData) == HAL_OK) {
        can_rx_cnt++; // 统计 RX 次数

        // 1. 将硬件拿到的原始数据极速打包
        raw_msg.id = RxHeader.StdId;
        raw_msg.dlc = RxHeader.DLC;
        for(int i=0; i<8; i++) {
            raw_msg.data[i] = RxData[i];
        }

        // 2. 使用专用的 FromISR 函数将数据压入接收队列
        xQueueSendFromISR(Queue_CAN_RX_t, &raw_msg, &xHigherPriorityTaskWoken);

        // 3. 立即触发 FreeRTOS 的上下文切换（如果 CanRX_Task 优先级更高，会瞬间跳过去执行）
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *hcan)
{
    if (hcan->Instance != CAN1)
    {
        return;
    }

    uint32_t error_code = HAL_CAN_GetError(hcan);
    uint32_t esr = hcan->Instance->ESR;

    can_last_error = error_code;

    can_last_tec =
        (uint8_t)((esr & CAN_ESR_TEC_Msk) >> CAN_ESR_TEC_Pos);

    can_last_rec =
        (uint8_t)((esr & CAN_ESR_REC_Msk) >> CAN_ESR_REC_Pos);

    /*
     * 同一次回调中可能同时存在ACK和TX_TERR，
     * 所以整个mask只累加一次，避免一帧重复计数。
     */
    if (error_code &
        (HAL_CAN_ERROR_ACK |
         HAL_CAN_ERROR_BR |
         HAL_CAN_ERROR_BD |
         HAL_CAN_ERROR_STF |
         HAL_CAN_ERROR_FOR |
         HAL_CAN_ERROR_CRC |
         HAL_CAN_ERROR_TX_TERR0 |
         HAL_CAN_ERROR_TX_TERR1 |
         HAL_CAN_ERROR_TX_TERR2 |
         HAL_CAN_ERROR_TX_ALST0 |
         HAL_CAN_ERROR_TX_ALST1 |
         HAL_CAN_ERROR_TX_ALST2))
    {
        can_tx_fail_cnt++;
    }

    if (error_code & HAL_CAN_ERROR_ACK)
    {
        can_ack_error_cnt++;
    }

    if (error_code &
        (HAL_CAN_ERROR_BR |
         HAL_CAN_ERROR_BD |
         HAL_CAN_ERROR_STF |
         HAL_CAN_ERROR_FOR |
         HAL_CAN_ERROR_CRC))
    {
        can_bit_error_cnt++;
    }

    if (error_code & HAL_CAN_ERROR_BOF)
    {
        can_busoff_cnt++;
    }

    /*
     * HAL的ErrorCode是累积OR保存的。
     * 不清除的话，下一次回调可能还包含旧错误。
     */
    HAL_CAN_ResetError(hcan);
}

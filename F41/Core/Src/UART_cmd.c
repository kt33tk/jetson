#include "UART_cmd.h"

extern QueueHandle_t Queue_Control_Cmd_t;

uint8_t data_receive[6];
uint8_t receive_index = 0;
extern uint8_t receive_byte;

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;

	if(huart->Instance == USART1)
	{
		if(receive_index == 0)
		{
			if(receive_byte == 0xAA)
			{
				data_receive[receive_index++] = receive_byte;
			}
		}
		else if(receive_index == 1)
		{
			if(receive_byte == 0x55)
			{
				data_receive[receive_index++] = receive_byte;
			}
			else if(receive_byte == 0xAA)
			{
				receive_index = 1;
			}
			else
			{
				receive_index = 0;
			}
		}
		else if(receive_index >= 2 && receive_index <= 4)
		{
			data_receive[receive_index++] = receive_byte;
		}
		else if(receive_index == 5)
		{
			data_receive[5] = receive_byte;
			receive_index = 0;

			if(data_receive[5] == 0xCC)
			{
				uint8_t sum = (data_receive[2] + data_receive[3]) & 0xFF;
				if(sum == data_receive[4])
				{
					ControlCommand_t new_cmd = {0};

					uint8_t command = data_receive[2];
					uint8_t target_percent = data_receive[3];

					uint8_t param_valid = 1;

					/* 检查命令范围 */
					if (command < CONTROL_CMD_AUTO ||
					    command > CONTROL_CMD_CLEAR_FAULT)
					{
					    param_valid = 0;
					}

					/* 只有MANUAL命令允许携带0~100的目标值 */
					if (command == CONTROL_CMD_MANUAL)
					{
					    if (target_percent > 100U)
					    {
					        param_valid = 0;
					    }
					}
					else
					{
					    /* AUTO、SAFE、CLEAR的目标值必须为0 */
					    if (target_percent != 0U)
					    {
					        param_valid = 0;
					    }
					}

					if (param_valid == 1U)
					{
					    new_cmd.command =
					        (Control_CommandCode_t)command;

					    new_cmd.target_percent = target_percent;
					    new_cmd.rx_counter = 0;
					    new_cmd.valid = 1;
					    new_cmd.timestamp = xTaskGetTickCountFromISR();

					    if (xQueueSendFromISR(
					            Queue_Control_Cmd_t,
					            &new_cmd,
					            &xHigherPriorityTaskWoken) != pdPASS)
					    {
					        /*
					         * ISR中不要printf。
					         * 后续可增加uart_cmd_queue_full_cnt。
					         */
					    }
					}
				}
			}
		}
		else
		{
			receive_index = 0;
		}
	}

	HAL_UART_Receive_IT(huart, &receive_byte, 1);
	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

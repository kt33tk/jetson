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
					new_cmd.target_mode = data_receive[2];
					new_cmd.manual_request = data_receive[3];
					new_cmd.has_new_cmd = 1;

					xQueueSendFromISR(Queue_Control_Cmd_t, &new_cmd, &xHigherPriorityTaskWoken);
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

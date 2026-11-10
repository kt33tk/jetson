#include "ADC_Sensor.h"

uint16_t adc_buffer[ADC_BUF_LEN];

extern ADC_HandleTypeDef hadc1;
extern TIM_HandleTypeDef htim2;

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    BaseType_t higher_priority_task_woken = pdFALSE;

    if (hadc->Instance == ADC1)
    {
        xSemaphoreGiveFromISR(
            AdcDmaDoneSemaphore,
            &higher_priority_task_woken
        );

        portYIELD_FROM_ISR(higher_priority_task_woken);
    }
}

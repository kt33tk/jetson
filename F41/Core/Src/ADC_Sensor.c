#include "ADC_Sensor.h"

uint16_t adc_buffer[ADC_BUF_LEN];

extern QueueHandle_t Queue_ADC_Raw_t;
extern ADC_HandleTypeDef hadc1;
extern TIM_HandleTypeDef htim2;

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    if (hadc->Instance == ADC1)
    {
        uint32_t sum = 0;
        for (int i = 0; i < ADC_BUF_LEN; i++)
        {
            sum += adc_buffer[i];
        }
        uint16_t adc_avg = sum / ADC_BUF_LEN; // 计算滑动/块均值

        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        // 将平均值发给 Sensor 任务
        xQueueSendFromISR(Queue_ADC_Raw_t, &adc_avg, &xHigherPriorityTaskWoken);

        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

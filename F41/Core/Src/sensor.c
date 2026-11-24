#include "sensor.h"

extern QueueHandle_t Queue_Sensor_TX_t;
extern QueueHandle_t Queue_Sensor_Control_t;

extern ADC_HandleTypeDef hadc1;
extern TIM_HandleTypeDef htim2;

extern TaskHandle_t WatchdogTaskHandle;

extern uint16_t adc_buffer[ADC_BUF_LEN];

void Sensor_Task(void *pvParameters)
{
    Sensor_Sample_t sample = {.power_status = POWER_I2C_ERROR};
    uint8_t last_fault = 0U;

    INA226_Init();
    HAL_TIM_Base_Start(&htim2);
    HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_buffer, ADC_BUF_LEN);

    for (;;)
    {
        if (xSemaphoreTake(AdcDmaDoneSemaphore, pdMS_TO_TICKS(200)) == pdTRUE)
        {
            uint32_t sum = 0U;

            for (uint32_t i = 0U; i < ADC_BUF_LEN; i++)
                sum += adc_buffer[i];

            uint16_t adc = sum / ADC_BUF_LEN;
            uint8_t fault = adc > 4000U ? 1U :
                            adc < 50U ? 2U : 0U;

            if (fault != last_fault && fault != 0U)
                sample.error_count++;

            last_fault = fault;
            sample.error_state = fault;
            sample.valid_flag = fault == 0U;
            sample.temperature = 0;

            if (sample.valid_flag)
            {
                float resistance = 10000.0f * adc / (4095.0f - adc);
                float kelvin = 1.0f / (1.0f / 298.15f + logf(resistance / 10000.0f) / 3950.0f);

                sample.temperature = (int16_t)((kelvin - 273.15f) * 10.0f);
            }
        }
        else
        {
            sample.valid_flag = 0U;
        }

        sample.power_status = INA226_Read(&sample.voltage_mv, &sample.current_ma);

        sample.timestamp = xTaskGetTickCount();

        if (sample.valid_flag)
            xEventGroupSetBits(SystemEventGroup, EVT_SENSOR_VALID);
        else
            xEventGroupClearBits(SystemEventGroup, EVT_SENSOR_VALID);

        DiagSnapshot_UpdateSensor(sample.temperature, sample.valid_flag, sample.timestamp);

        xQueueOverwrite(Queue_Sensor_Control_t, &sample);
        xQueueOverwrite(Queue_Sensor_TX_t, &sample);

        xTaskNotify(WatchdogTaskHandle, HEARTBEAT_SENSOR, eSetBits);
    }
}

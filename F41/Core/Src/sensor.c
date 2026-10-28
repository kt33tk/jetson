#include "sensor.h"

extern QueueHandle_t Queue_Sensor_TX_t;
extern QueueHandle_t Queue_Sensor_Control_t;
extern QueueHandle_t Queue_ADC_Raw_t;

extern ADC_HandleTypeDef hadc1;
extern TIM_HandleTypeDef htim2;

extern TaskHandle_t WatchdogTaskHandle;

extern uint16_t adc_buffer[ADC_BUF_LEN];

void Sensor_Task(void *pvParameters)
{
	uint16_t raw_adc_avg = 0;
	uint8_t last_fault_state = 0; // 用于 DTC 边沿触发检测
	Sensor_Sample_t sample_data = {0};

	HAL_TIM_Base_Start(&htim2);
	HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_buffer, ADC_BUF_LEN);

	for(;;)
	{
		if (xQueueReceive(Queue_ADC_Raw_t, &raw_adc_avg, portMAX_DELAY) == pdTRUE)
		{
		    sample_data.timestamp = xTaskGetTickCount();
		    uint8_t current_fault = 0;

		    // 1. 物理电气边界与故障判定
		    if (raw_adc_avg > 4000) {
		        current_fault = 1; // 开路故障 (NTC 拔掉，电压被上拉至接近 3.3V)
		    } else if (raw_adc_avg < 50) {
		        current_fault = 2; // 短路故障 (NTC 探头短路，电压被拉低至 0V)
		    }

		    // 2. 边沿触发 DTC 记录 (仅在故障状态发生翻转时记录一次)
		    if (current_fault != last_fault_state) {
		        if (current_fault != 0) {
		            sample_data.error_state = current_fault;
		            sample_data.error_count++;
		        }
		        last_fault_state = current_fault;
		    }

		    // 3. 数据处理与温度解算
		    if (current_fault != 0) {
		        sample_data.valid_flag = 0;  // 故障发生，废弃温度数据，联动控制系统进入 SAFE
		        sample_data.temperature = 0;
		    } else {
		        sample_data.valid_flag = 1;

		        // (a) 根据分压公式求 NTC 实时阻值：R_NTC = 10000 * ADC / (4095 - ADC)
		        float r_ntc = 10000.0f * (float)raw_adc_avg / (4095.0f - (float)raw_adc_avg);

		        // (b) 运用 B 值公式 (B=3950, 10k=298.15K) 计算开尔文温度 Tk
		        float ln_r_ratio = log(r_ntc / 10000.0f);
		        float tk = 1.0f / ((1.0f / 298.15f) + (1.0f / 3950.0f) * ln_r_ratio);

		        // (c) 转换为摄氏度 Tc
		        float tc = tk - 273.15f;

		        // (d) 统一使用 0.1°C 整数格式（如 25.4°C -> 254）
		        sample_data.temperature = (int16_t)(tc * 10.0f);
		    }

		    xQueueOverwrite(Queue_Sensor_Control_t, &sample_data);
		    xQueueOverwrite(Queue_Sensor_TX_t, &sample_data);

		    xTaskNotify(WatchdogTaskHandle, HEARTBEAT_SENSOR, eSetBits);
		}

	}
}

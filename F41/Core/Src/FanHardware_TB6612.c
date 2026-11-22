#include "FanHardware.h"

/*
 * 暂定参数，必须根据实物标定：
 *
 * 1040 = 假设编码器 13 线、减速比 20:1、AB 四倍频。
 * 如果实测值不同，改成输出轴每转实际计数。
 *
 * 350 是暂定的最高目标转速，单位 RPM。
 */
#define MG310_COUNTS_PER_OUTPUT_REV  1040U
#define MG310_MAX_TARGET_RPM         350U

extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim4;

static uint16_t previous_count;
static uint32_t pulse_total;

uint8_t FanHw_Init(void)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_10, GPIO_PIN_RESET);

    if (HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1) != HAL_OK)
    {
        return 0U;
    }

    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0U);

    if (HAL_TIM_Encoder_Start( &htim4, TIM_CHANNEL_ALL) != HAL_OK)
    {
        return 0U;
    }

    previous_count = (uint16_t)__HAL_TIM_GET_COUNTER(&htim4);

    pulse_total = 0U;
    FanHw_Stop();

    return 1U;
}

void FanHw_SetDuty(uint8_t percent)
{
    uint32_t period;

    if (percent > 100U)
    {
        percent = 100U;
    }

    if (percent == 0U)
    {
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0U);

        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10 | GPIO_PIN_0 | GPIO_PIN_1, GPIO_PIN_RESET);
        return;
    }

    /* 固定正转：AIN1=1，AIN2=0。 */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);

    period = __HAL_TIM_GET_AUTORELOAD(&htim1) + 1U;

    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, period * percent / 100U);

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_SET);
}

void FanHw_Stop(void)
{
    FanHw_SetDuty(0U);
}

uint32_t FanHw_ReadPulses(void)
{
    uint16_t now = (uint16_t)__HAL_TIM_GET_COUNTER(&htim4);

    int32_t delta = (int16_t)(uint16_t)(now - previous_count);

    previous_count = now;

    if (delta < 0)
    {
        delta = -delta;
    }

    pulse_total += (uint32_t)delta;
    return pulse_total;
}

uint32_t FanHw_PulsesPerRevolution(void)
{
    return MG310_COUNTS_PER_OUTPUT_REV;
}

uint32_t FanHw_MaxTargetRpm(void)
{
    return MG310_MAX_TARGET_RPM;
}

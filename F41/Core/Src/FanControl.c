#include "FanControl.h"

static uint8_t running;                //控制有没有进入启动或运行阶段
static uint32_t start_ms;              //风扇从停机开始启动的时刻
static uint32_t sample_ms;             //当前测速窗口的起始时刻
static uint32_t sample_pulses;         //测速窗口开始时的累计编码器计数
static uint32_t low_speed_since;       //第一次测到低速的时刻
static uint32_t rpm;                   //最近一次算出的实际转速
static float integral;                 //PI 累积下来的占空比修正量
static float previous_error;           //上一轮的转速误差

uint8_t FanControl_Init(void)
{
    running = 0U;
    rpm = 0U;
    integral = 0.0f;

    return FanHw_Init();
}

uint32_t FanControl_GetRpm(void)
{
    return rpm;
}

uint8_t FanControl_Update(uint8_t target_percent, uint32_t now_ms)
{
    uint32_t elapsed;                //距离上一次测速过了多久
    uint32_t pulses;                 //这次读到的累计编码器计数
    uint32_t target_rpm;             //这次希望风扇达到的转速
    uint32_t pulses_per_rev;         //风扇输出轴转一圈对应多少计数
    float error;                     //目标转速和实测转速的差
    float duty;                      //本轮算出来要给电机的 PWM 占空比
    float dt;                        //同一个采样间隔换算成秒

    if (target_percent == 0U)
    {
        if (running != 0U)
        {
            FanHw_Stop();
        }

        running = 0U;
        rpm = 0U;
        integral = 0.0f;
        low_speed_since = 0U;
        return 0U;
    }

    if (target_percent > 100U)
    {
        target_percent = 100U;
    }

    pulses_per_rev = FanHw_PulsesPerRevolution();

    if (pulses_per_rev == 0U)
    {
        FanHw_Stop();
        return 1U;
    }

    /*
     * 每次任务调用都读取一次编码器。
     * 这样启动期间也不会积累过长的 TIM4 计数差。
     */
    pulses = FanHw_ReadPulses();

    if (running == 0U)
    {
        running = 1U;
        start_ms = now_ms;
        sample_ms = now_ms;
        sample_pulses = pulses;
        low_speed_since = 0U;
        integral = 0.0f;
        previous_error = 0.0f;

        FanHw_SetDuty(100U);
        return 0U;
    }

    /* 从停机开始启动时，100% PWM 保持 500ms。 */
    if (now_ms - start_ms < 500U)
    {
        FanHw_SetDuty(100U);
        return 0U;
    }

    elapsed = now_ms - sample_ms;

    if (elapsed < 100U)
    {
        return 0U;
    }

    rpm =(uint32_t)(((uint64_t)(pulses - sample_pulses) * 60000ULL) / ((uint64_t)pulses_per_rev * elapsed));

    sample_pulses = pulses;
    sample_ms = now_ms;

    target_rpm = (uint32_t)target_percent * FanHw_MaxTargetRpm() / 100U;

    error = (float)target_rpm - (float)rpm;
    dt = (float)elapsed / 1000.0f;

    /*
     * 初次调试先用 PI。
     * Kp=0.10、Ki=0.03 只是起始值；
     * Kd 暂时为 0，测速稳定后再决定是否启用。
     */
    integral += 0.03f * error * dt;

    if (integral > 40.0f)
    {
        integral = 40.0f;
    }

    if (integral < -40.0f)
    {
        integral = -40.0f;
    }

    duty = 0.7f * (float)target_percent + 0.10f * error + integral + 0.0f * (error - previous_error) / dt;

    previous_error = error;

    if (duty > 100.0f)
    {
        duty = 100.0f;
    }

    if (duty < 0.0f)
    {
        duty = 0.0f;
    }

    FanHw_SetDuty((uint8_t)duty);

    /*
     * 启动结束后，连续 500ms 低于目标转速的 20%，
     * 才返回故障。
     */
    if (rpm < target_rpm / 5U)
    {
        if (low_speed_since == 0U)
        {
            low_speed_since = now_ms;
        }
        else if (now_ms - low_speed_since >= 500U)
        {
            FanHw_Stop();
            running = 0U;
            return 1U;
        }
    }
    else
    {
        low_speed_since = 0U;
    }

    return 0U;
}

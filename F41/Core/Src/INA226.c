#include "INA226.h"

extern I2C_HandleTypeDef hi2c1;

volatile HAL_StatusTypeDef ina_last_status;

HAL_StatusTypeDef INA226_Init(void)
{
    uint8_t config[2] = {0x41, 0x27};

    ina_last_status = HAL_I2C_Mem_Write(
        &hi2c1, INA_ADDR, 0x00, I2C_MEMADD_SIZE_8BIT,
        config, 2, 10
    );

    return ina_last_status;
}

uint8_t INA226_Read(uint16_t *voltage_mv, int16_t *current_ma)
{
    uint8_t data[2];      //INA226读取数据
    uint16_t bus;         //总线电压寄存器的原始数值
    int16_t shunt;        //分流电压寄存器的原始数值

    *voltage_mv = 0xFFFFU;
    *current_ma = INT16_MIN;

    ina_last_status = HAL_I2C_Mem_Read(
        &hi2c1, INA_ADDR, 0x02, I2C_MEMADD_SIZE_8BIT,
        data, 2, 10
    );
    if (ina_last_status != HAL_OK)
        return POWER_I2C_ERROR;

    bus = ((uint16_t)data[0] << 8) | data[1];

    ina_last_status = HAL_I2C_Mem_Read(
        &hi2c1, INA_ADDR, 0x01, I2C_MEMADD_SIZE_8BIT,
        data, 2, 10
    );
    if (ina_last_status != HAL_OK)
        return POWER_I2C_ERROR;

    shunt = (int16_t)(((uint16_t)data[0] << 8) | data[1]);

    /* 当前实验只用0～12V；同时拒绝分流ADC饱和值。 */
    if (bus > 9600U || shunt >= 32760 || shunt <= -32760)
        return POWER_RANGE_ERROR;

    *voltage_mv = (uint32_t)bus * 5U / 4U;
    *current_ma = shunt / 40;

    return POWER_OK;
}

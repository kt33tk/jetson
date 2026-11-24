#ifndef INA226_H
#define INA226_H

#include "main.h"

#define INA_ADDR (0x40U << 1)

enum
{
    POWER_OK = 0,
    POWER_I2C_ERROR = 1,
    POWER_RANGE_ERROR = 2,
    POWER_STALE = 3
};

extern volatile HAL_StatusTypeDef ina_last_status;

HAL_StatusTypeDef INA226_Init(void);
uint8_t INA226_Read(uint16_t *voltage_mv, int16_t *current_ma);

#endif

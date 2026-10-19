#ifndef __CRC8_H
#define __CRC8_H

#include <stdint.h>
#include <stdio.h>

uint8_t Calculate_CRC8_J1850(uint8_t *data, uint16_t length);
void Test_CRC8_J1850(void);

#endif
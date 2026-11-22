#ifndef __CRC32_H
#define __CRC32_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

uint32_t CRC32_Calculate(const void *data, size_t length);
uint8_t CRC32_SelfTest(void);

#endif

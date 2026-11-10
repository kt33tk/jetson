#include "CRC32.h"

uint32_t CRC32_Calculate(const void *data, size_t length)
{
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFFUL;

    for (size_t i = 0; i < length; i++)
    {
        crc ^= bytes[i];

        for (uint8_t bit = 0; bit < 8U; bit++)
        {
            if ((crc & 1UL) != 0UL)
            {
                crc = (crc >> 1) ^ 0xEDB88320UL;
            }
            else
            {
                crc >>= 1;
            }
        }
    }

    return crc ^ 0xFFFFFFFFUL;
}

uint8_t CRC32_SelfTest(void)
{
    static const char test_data[] = "123456789";

    return CRC32_Calculate(test_data, strlen(test_data))
           == 0xCBF43926UL;
}

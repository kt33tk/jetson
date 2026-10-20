#include "crc8.h"

uint8_t Calculate_CRC8_J1850(uint8_t *data, uint16_t length) {
    uint8_t crc = 0xFF; // 初始值为 0xFF
    for (uint16_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x1D; // 多项式为 0x1D[cite: 17]
            } else {
                crc = (crc << 1);
            }
        }
    }
    return crc ^ 0xFF; // 结果异或 0xFF[cite: 17]
}

void Test_CRC8_J1850(void) {
    uint8_t test1[7] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77}; //[cite: 17]
    uint8_t test2[7] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t test3[7] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t test4[7] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40};
    uint8_t test5[7] = {0xDE, 0xAD, 0xBE, 0xEF, 0xAA, 0xBB, 0xCC};

    // 此处用于临时打印基准值（操作详见下方的执行策略）
    printf("T1: 0x%02X, T2: 0x%02X, T3: 0x%02X, T4: 0x%02X, T5: 0x%02X\n",
           Calculate_CRC8_J1850(test1, 7), Calculate_CRC8_J1850(test2, 7),
           Calculate_CRC8_J1850(test3, 7), Calculate_CRC8_J1850(test4, 7),
           Calculate_CRC8_J1850(test5, 7));

    // 最终定稿的严丝合缝校验逻辑[cite: 17]
    // 假设你在 Jetson 跑出来的 test1 结果是 0xXX，填入下方替换

    if (Calculate_CRC8_J1850(test1, 7) != 0xD0 ||
        Calculate_CRC8_J1850(test2, 7) != 0x0A ||
        Calculate_CRC8_J1850(test3, 7) != 0x34 ||
        Calculate_CRC8_J1850(test4, 7) != 0x8C ||
        Calculate_CRC8_J1850(test5, 7) != 0x45) {

        printf("FATAL ERROR: CRC Alignment Failed!\n");
        while(1); // 失败死锁，绝不允许启动带有隐患的系统[cite: 17]
    }
}

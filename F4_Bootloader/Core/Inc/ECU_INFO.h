#ifndef ECU_INFO_H
#define ECU_INFO_H

#include <stdint.h>

#define ECU_HARDWARE_ID     0x04070001UL

#define APP_INFO_ADDRESS    0x08010200UL
#define APP_INFO_MAGIC      0x45435549UL
#define APP_INFO_FORMAT     1U

#define APP_SW_MAJOR        1U
#define APP_SW_MINOR        0U
#define APP_SW_PATCH        0U

#define BOOT_SW_MAJOR       1U
#define BOOT_SW_MINOR       1U
#define BOOT_SW_PATCH       0U

typedef struct
{
    uint32_t magic;           //判断是否为预期的 App 信息块
    uint32_t hardware_id;     //ECU 硬件型号标识
    uint8_t major;            //App 软件主版本号
    uint8_t minor;            //App 软件次版本号
    uint8_t patch;            //App 软件修订版本号
    uint8_t format;           //AppInfo 结构体的格式版本
} AppInfo_t;

_Static_assert(sizeof(AppInfo_t) == 12U, "Unexpected AppInfo size");

#endif

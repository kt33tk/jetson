#include "ECU_INFO.h"

__attribute__((section(".app_info"), used))          //将包含硬件 ID 和 App 版本信息的 12 字节结构体，存放到内部 Flash 的 0x08010200 地址
const AppInfo_t g_app_info =
{
    .magic = APP_INFO_MAGIC,
    .hardware_id = ECU_HARDWARE_ID,
    .major = APP_SW_MAJOR,
    .minor = APP_SW_MINOR,
    .patch = APP_SW_PATCH,
    .format = APP_INFO_FORMAT
};

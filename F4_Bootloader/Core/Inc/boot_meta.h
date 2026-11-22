#ifndef BOOT_META_H
#define BOOT_META_H

#include <stdint.h>

#define APP_BASE       0x08010000UL
#define APP_END        0x08080000UL
#define META_BASE      0x0800C000UL
#define META_MAGIC     0x4D455441UL  /* "META" */

#define META_DOWNLOADING  1UL
#define META_VALID        2UL
#define META_INVALID      3UL

typedef struct
{
    uint32_t magic;
    uint32_t state;
    uint32_t image_size;
    uint32_t image_crc32;
    uint32_t meta_crc32;
} BootMeta_t;

#endif

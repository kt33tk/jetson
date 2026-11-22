#ifndef BOOT_IMAGE_H
#define BOOT_IMAGE_H

#include "BOOT_FLASH.h"
#include "boot_meta.h"
#include "CRC32.h"
#include <stddef.h>
#include <string.h>
#include <stdint.h>

#define IMAGE_A        0x100000UL          //A区镜像头
#define IMAGE_B        0x200000UL          //B区镜像头
#define IMAGE_DATA     0x001000UL          //固件数据

#define IMAGE_MAGIC    0x4657494DUL
#define IMAGE_VERSION  1UL
#define IMAGE_COMMIT   0x434F4D54UL

typedef struct
{
    uint32_t magic;         //镜像头及其结构格式
    uint32_t version;       //镜像头及其结构格式
    uint32_t size;          //正文长度
    uint32_t crc;           //正文CRC
    uint32_t header_crc;    //镜像头CRC
    uint32_t commit;        //提交标记
} ImageHeader;

typedef enum
{
    IMAGE_IDLE,
    IMAGE_ERASE_A,
    IMAGE_VERIFY_A,
    IMAGE_SEAL_A,
    IMAGE_ERASE_B,
    IMAGE_COPY_B,
    IMAGE_VERIFY_B,
    IMAGE_SEAL_B,
    IMAGE_MARK_INTERNAL,
    IMAGE_ERASE_INTERNAL,
    IMAGE_COPY_INTERNAL,
    IMAGE_VERIFY_INTERNAL,
    IMAGE_COMMIT_INTERNAL,
    IMAGE_DONE,
    IMAGE_ERROR
} ImageStage;

extern volatile ImageStage image_stage;

uint8_t Image_AppValid(void);
uint8_t Image_StartDownload(uint32_t size);
uint8_t Image_Write(uint32_t offset, const uint8_t *data, uint16_t length);
uint8_t Image_StartInstall(uint32_t size, uint32_t crc);
uint8_t Image_StartRestore(void);

/* 0：进行中；1：完成；-1：失败。 */
int Image_Poll(void);

#endif

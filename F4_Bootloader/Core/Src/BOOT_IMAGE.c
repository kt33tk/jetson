#include "BOOT_IMAGE.h"

volatile ImageStage image_stage = IMAGE_IDLE;

static ImageHeader target;              //将写入片内Flash的镜像头
static ImageHeader backup;              //就镜像头

static uint8_t buffer[256];             //临时缓冲区
static uint32_t position;               //处理到了的字节位置
static uint32_t running_crc;            //当前校验阶段的 CRC 累计值
static uint32_t source_slot;            //写入片内 Flash 时从哪里读
static uint32_t erase_sector;           //下次擦除的片内 Flash 扇区编号
static uint8_t restoring;               //正常安装还是从B区恢复旧APP

_Static_assert(sizeof(ImageHeader) == 24U, "Unexpected image header size");

_Static_assert(IMAGE_DATA + (APP_END - APP_BASE) <= 0x80000UL, "Image does not fit external slot");

static void Begin(ImageStage stage)
{
    position = 0U;
    running_crc = 0xFFFFFFFFUL;
    image_stage = stage;
}

static uint32_t CrcFeed(uint32_t crc, const uint8_t *data, uint32_t length)      //将该块数据算入总CRC
{
    while (length-- != 0U)
    {
        crc ^= *data++;    //取字节、异或后移动指针

        for (uint8_t bit = 0U; bit < 8U; bit++)
        {
            crc = (crc >> 1) ^ ((crc & 1U) ? 0xEDB88320UL : 0U);
        }
    }

    return crc;
}

static uint8_t VectorValid(const uint32_t vector[2], uint32_t size)
{
    uint32_t msp = vector[0];
    uint32_t reset = vector[1];
    uint32_t entry = reset & ~1UL;

    return msp >= 0x20000000UL &&      //初始栈指针
           msp <= 0x2001FFF8UL &&
           (msp & 7U) == 0U &&         //初始栈指针8字节对齐
           (reset & 1U) != 0U &&       //检验最低位是否为1
           entry >= APP_BASE &&
           entry < APP_BASE + size;
}

uint8_t Image_AppValid(void)
{
    const BootMeta_t *meta = (const BootMeta_t *)META_BASE;

    if (meta->magic != META_MAGIC ||
        meta->state != META_VALID ||
        meta->image_size < 8U ||
        meta->image_size > APP_END - APP_BASE)
    {
        return 0U;
    }

    if (CRC32_Calculate(meta, offsetof(BootMeta_t, meta_crc32)) != meta->meta_crc32)
    {
        return 0U;
    }

    return VectorValid((const uint32_t *)APP_BASE, meta->image_size) && CRC32_Calculate((const void *)APP_BASE, meta->image_size) == meta->image_crc32;
}

static ImageHeader MakeHeader(uint32_t size, uint32_t crc)
{
    ImageHeader header = {
        IMAGE_MAGIC,
        IMAGE_VERSION,
        size,
        crc,
        0U,
        IMAGE_COMMIT
    };

    header.header_crc = CRC32_Calculate(&header, offsetof(ImageHeader, header_crc));

    return header;
}

static uint8_t ReadHeader(uint32_t slot, ImageHeader *header)
{
    if (!Bf_Read(slot, header, sizeof(*header)))
        return 0U;

    return header->magic == IMAGE_MAGIC &&
           header->version == IMAGE_VERSION &&
           header->size >= 8U &&
           header->size <= APP_END - APP_BASE &&
           header->commit == IMAGE_COMMIT &&
           header->header_crc == CRC32_Calculate(header, offsetof(ImageHeader, header_crc));
}

static uint8_t SealHeader(uint32_t slot, const ImageHeader *header)
{
    ImageHeader check;

    /* 先写主体，最后写提交标记。 */
    if (!Bf_Write(slot, header, offsetof(ImageHeader, commit)))
        return 0U;

    if (!Bf_Write(slot + offsetof(ImageHeader, commit), &header->commit, sizeof(header->commit)))
    {
        return 0U;
    }

    return ReadHeader(slot, &check) && memcmp(&check, header, sizeof(check)) == 0;
}

static uint8_t EraseInternal(uint32_t sector)
{
    FLASH_EraseInitTypeDef erase = {
        .TypeErase = FLASH_TYPEERASE_SECTORS,
        .Sector = sector,
        .NbSectors = 1U,
        .VoltageRange = FLASH_VOLTAGE_RANGE_3
    };

    uint32_t failed_sector;

    if (HAL_FLASH_Unlock() != HAL_OK)
        return 0U;

    HAL_StatusTypeDef result = HAL_FLASHEx_Erase(&erase, &failed_sector);

    HAL_FLASH_Lock();
    return result == HAL_OK;
}

static uint8_t WriteInternal(uint32_t address, const uint8_t *data, uint32_t length)
{
    HAL_StatusTypeDef result = HAL_OK;

    if (HAL_FLASH_Unlock() != HAL_OK)
        return 0U;

    for (uint32_t i = 0U; i < length; i++)
    {
        result = HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, address + i, data[i]);

        if (result != HAL_OK)
            break;
    }

    HAL_FLASH_Lock();

    return result == HAL_OK && memcmp((const void *)address, data, length) == 0;
}

static uint8_t WriteMeta(uint32_t state, const ImageHeader *header)
{
    BootMeta_t meta = {
        .magic = META_MAGIC,
        .state = state,
        .image_size = header->size,
        .image_crc32 = header->crc,
        .meta_crc32 = 0U
    };

    meta.meta_crc32 = CRC32_Calculate(&meta, offsetof(BootMeta_t, meta_crc32));

    if (!EraseInternal(FLASH_SECTOR_3))
        return 0U;

    return WriteInternal(META_BASE, (const uint8_t *)&meta, sizeof(meta));
}

/*
 * 每次只校验最多256字节。
 * internal=0：读取外部Flash；internal=1：读取内部Flash。
 */
static int CheckStep(uint32_t base, const ImageHeader *header, uint8_t internal)
{
    uint32_t count = header->size - position;

    if (count > sizeof(buffer))
        count = sizeof(buffer);

    if (internal)
    {
        memcpy(buffer, (const void *)(base + position), count);
    }
    else if (!Bf_Read(base + position, buffer, count))
    {
        return -1;
    }

    if (position == 0U)
    {
        uint32_t vector[2];
        memcpy(vector, buffer, sizeof(vector));

        if (!VectorValid(vector, header->size))
            return -1;
    }

    running_crc = CrcFeed(running_crc, buffer, count);
    position += count;

    if (position != header->size)
        return 0;

    return (running_crc ^ 0xFFFFFFFFUL) == header->crc ? 1 : -1;
}

uint8_t Image_StartDownload(uint32_t size)
{
    if (size < 8U || size > APP_END - APP_BASE || !Image_AppValid())
    {
        return 0U;
    }

    target = MakeHeader(size, 0U);
    restoring = 0U;
    Begin(IMAGE_ERASE_A);

    return 1U;
}

uint8_t Image_Write(uint32_t offset, const uint8_t *data, uint16_t length)
{
    uint32_t address = IMAGE_A + IMAGE_DATA + offset;

    if (!Bf_Write(address, data, length))
        return 0U;

    return Bf_Read(address, buffer, length) && memcmp(buffer, data, length) == 0;
}

uint8_t Image_StartInstall(uint32_t size, uint32_t crc)
{
    if (!Image_AppValid())
        return 0U;

    const BootMeta_t *meta = (const BootMeta_t *)META_BASE;

    target = MakeHeader(size, crc);
    backup = MakeHeader(meta->image_size, meta->image_crc32);
    restoring = 0U;

    Begin(IMAGE_VERIFY_A);
    return 1U;
}

uint8_t Image_StartRestore(void)
{
    if (!ReadHeader(IMAGE_B, &backup))
        return 0U;

    target = backup;
    restoring = 1U;
    source_slot = IMAGE_B;

    Begin(IMAGE_VERIFY_B);
    return 1U;
}

int Image_Poll(void)
{
    uint32_t count;
    uint32_t erase_length;
    int result;

    switch (image_stage)
    {
    case IMAGE_ERASE_A:
    case IMAGE_ERASE_B:
    {
        uint8_t is_a = image_stage == IMAGE_ERASE_A;
        uint32_t slot = is_a ? IMAGE_A : IMAGE_B;
        uint32_t size = is_a ? target.size : backup.size;

        erase_length = IMAGE_DATA + ((size + 4095U) & ~4095UL);

        if (!Bf_Erase(slot + position))
            goto failed;

        position += 4096U;

        if (position >= erase_length)
            Begin(is_a ? IMAGE_DONE : IMAGE_COPY_B);

        break;
    }

    case IMAGE_VERIFY_A:
        result = CheckStep(IMAGE_A + IMAGE_DATA, &target, 0U);

        if (result < 0)
            goto failed;

        if (result > 0)
            Begin(IMAGE_SEAL_A);

        break;

    case IMAGE_SEAL_A:
        if (!SealHeader(IMAGE_A, &target))
            goto failed;

        Begin(IMAGE_ERASE_B);
        break;

    case IMAGE_COPY_B:
        count = backup.size - position;

        if (count > sizeof(buffer))
            count = sizeof(buffer);

        memcpy(buffer, (const void *)(APP_BASE + position), count);

        if (!Bf_Write(IMAGE_B + IMAGE_DATA + position, buffer, count))
        {
            goto failed;
        }

        position += count;

        if (position == backup.size)
            Begin(IMAGE_VERIFY_B);

        break;

    case IMAGE_VERIFY_B:
        result = CheckStep(IMAGE_B + IMAGE_DATA, &backup, 0U);

        if (result < 0)
            goto failed;

        if (result > 0)
        {
            if (restoring)
                Begin(IMAGE_MARK_INTERNAL);
            else
                Begin(IMAGE_SEAL_B);
        }

        break;

    case IMAGE_SEAL_B:
        if (!SealHeader(IMAGE_B, &backup))
            goto failed;

        source_slot = IMAGE_A;
        Begin(IMAGE_MARK_INTERNAL);
        break;

    case IMAGE_MARK_INTERNAL:
        /*
         * 到这里，B区已经完整校验并提交。
         * 现在才允许使内部Metadata失效。
         */
        if (!WriteMeta(META_DOWNLOADING, &target))
            goto failed;

        erase_sector = FLASH_SECTOR_4;
        Begin(IMAGE_ERASE_INTERNAL);
        break;

    case IMAGE_ERASE_INTERNAL:
        if (!EraseInternal(erase_sector))
            goto failed;

        erase_sector++;

        if (erase_sector > FLASH_SECTOR_7)
            Begin(IMAGE_COPY_INTERNAL);

        break;

    case IMAGE_COPY_INTERNAL:
        count = target.size - position;

        if (count > sizeof(buffer))
            count = sizeof(buffer);

        if (!Bf_Read(source_slot + IMAGE_DATA + position, buffer, count))
        {
            goto failed;
        }

        if (!WriteInternal(APP_BASE + position, buffer, count))
            goto failed;

        position += count;

        if (position == target.size)
            Begin(IMAGE_VERIFY_INTERNAL);

        break;

    case IMAGE_VERIFY_INTERNAL:
        result = CheckStep(APP_BASE, &target, 1U);

        if (result < 0)
            goto failed;

        if (result > 0)
            Begin(IMAGE_COMMIT_INTERNAL);

        break;

    case IMAGE_COMMIT_INTERNAL:
        if (!WriteMeta(META_VALID, &target))
            goto failed;

        Begin(IMAGE_DONE);
        break;

    case IMAGE_IDLE:
    case IMAGE_DONE:
        return 1;

    case IMAGE_ERROR:
        return -1;
    }

    return 0;

failed:
    image_stage = IMAGE_ERROR;
    return -1;
}

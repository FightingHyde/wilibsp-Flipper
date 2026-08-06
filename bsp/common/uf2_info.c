#include "common/uf2_info.h"

static uint32_t crc32_ieee(const void *data, size_t size) {
    const uint8_t *p = (const uint8_t *)data;
    uint32_t crc = 0xffffffffu;
    while (size--) {
        crc ^= *p++;
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return crc ^ 0xffffffffu;
}

uint32_t fw2app_uf2_info_crc(const fw2app_uf2_info_t *info) {
    return crc32_ieee(info, offsetof(fw2app_uf2_info_t, crc32));
}

static bool bounded(const char *s, size_t n) {
    while (n--)
        if (*s++ == '\0') return true;
    return false;
}

bool fw2app_uf2_info_valid(const fw2app_uf2_info_t *info) {
    return info->magic0 == FW2APP_UF2_INFO_MAGIC0 &&
           info->magic1 == FW2APP_UF2_INFO_MAGIC1 &&
           info->struct_version == FW2APP_UF2_INFO_VERSION &&
           info->kind == FW2APP_UF2_KIND_APP &&
           info->reserved0 == 0 &&
           info->reserved1 == 0 &&
           info->crc32 == fw2app_uf2_info_crc(info) &&
           bounded(info->name, sizeof info->name) &&
           bounded(info->description, sizeof info->description) &&
           bounded(info->build, sizeof info->build);
}

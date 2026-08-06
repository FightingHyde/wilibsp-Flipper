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

static bool printable_ascii(const char *s, size_t n, bool allow_empty) {
    if (!allow_empty && (n == 0 || *s == '\0')) return false;
    while (n--) {
        const unsigned char ch = (unsigned char)*s++;
        if (ch == '\0') return true;
        if (ch < 0x20u || ch > 0x7eu) return false;
    }
    return false;
}

bool fw2app_uf2_info_valid(const fw2app_uf2_info_t *info) {
    return info->magic0 == FW2APP_UF2_INFO_MAGIC0 &&
           info->magic1 == FW2APP_UF2_INFO_MAGIC1 &&
           info->struct_version == FW2APP_UF2_INFO_VERSION &&
           info->kind == FW2APP_UF2_KIND_APP &&
           info->reserved0 == 0 &&
           info->reserved1 == 0 &&
           info->app_version <= 999u &&
           info->crc32 == fw2app_uf2_info_crc(info) &&
           printable_ascii(info->name, sizeof info->name, false) &&
           printable_ascii(info->description, sizeof info->description, false) &&
           printable_ascii(info->build, sizeof info->build, true);
}

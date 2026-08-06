#ifndef FW2APP_UF2_INFO_H
#define FW2APP_UF2_INFO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Eight bytes: "FW2AINFO" in a raw UF2 payload. */
#define FW2APP_UF2_INFO_MAGIC0 0x41325746u /* "FW2A" */
#define FW2APP_UF2_INFO_MAGIC1 0x4F464E49u /* "INFO" */

#define FW2APP_UF2_INFO_VERSION 1u
#define FW2APP_UF2_KIND_APP 0u
#define FW2APP_UF2_NAME_LEN 32u
#define FW2APP_UF2_DESC_LEN 128u
#define FW2APP_UF2_BUILD_LEN 32u

typedef struct {
    uint32_t magic0;
    uint32_t magic1;
    uint16_t struct_version;
    uint8_t kind;
    uint8_t reserved0;
    uint16_t app_version;
    uint16_t reserved1;
    char name[FW2APP_UF2_NAME_LEN];
    char description[FW2APP_UF2_DESC_LEN];
    char build[FW2APP_UF2_BUILD_LEN];
    uint32_t build_ts;
    uint32_t crc32;
} fw2app_uf2_info_t;

_Static_assert(sizeof(fw2app_uf2_info_t) == 216,
               "fw2app_uf2_info_t layout changed");
_Static_assert(offsetof(fw2app_uf2_info_t, crc32) == 212,
               "crc32 must remain the final field");

uint32_t fw2app_uf2_info_crc(const fw2app_uf2_info_t *info);
bool fw2app_uf2_info_valid(const fw2app_uf2_info_t *info);

#endif

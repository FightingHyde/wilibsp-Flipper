#include <assert.h>
#include <string.h>
#include "common/uf2_info.h"

static fw2app_uf2_info_t valid_info(void) {
    fw2app_uf2_info_t info = {0};
    info.magic0 = FW2APP_UF2_INFO_MAGIC0;
    info.magic1 = FW2APP_UF2_INFO_MAGIC1;
    info.struct_version = FW2APP_UF2_INFO_VERSION;
    info.kind = FW2APP_UF2_KIND_APP;
    info.app_version = 7;
    strcpy(info.name, "demo");
    strcpy(info.description, "Demo application");
    info.crc32 = fw2app_uf2_info_crc(&info);
    return info;
}

int main(void) {
    fw2app_uf2_info_t info = valid_info();
    assert(fw2app_uf2_info_valid(&info));
    info.app_version = 1000;
    info.crc32 = fw2app_uf2_info_crc(&info);
    assert(!fw2app_uf2_info_valid(&info));
    info = valid_info();
    info.name[0] = '\0';
    info.crc32 = fw2app_uf2_info_crc(&info);
    assert(!fw2app_uf2_info_valid(&info));
    info = valid_info();
    info.description[0] = '\x1f';
    info.crc32 = fw2app_uf2_info_crc(&info);
    assert(!fw2app_uf2_info_valid(&info));
    info = valid_info();
    memset(info.build, 'x', sizeof info.build);
    info.crc32 = fw2app_uf2_info_crc(&info);
    assert(!fw2app_uf2_info_valid(&info));
    info = valid_info();
    info.crc32 ^= 1u;
    assert(!fw2app_uf2_info_valid(&info));
    return 0;
}
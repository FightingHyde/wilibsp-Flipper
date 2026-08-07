// hello_usbdrive — on-hardware smoke test for the harvested USB host MSC
// stack (bsp/usbhost) + FatFs. LCD + RTT status. Powers the HP1/HP2 ports, polls the
// host stack, and on every mount edge prints the volume root listing (the
// usb_store DIAGs) plus a non-recursive count of *.ir files at the volume
// root as a FatFs read exercise.
// Pass criteria with a FAT32 stick seated: "mount OK" + root listing within
// a few seconds of boot; pull/replug -> "drive removed" then a clean
// remount. No stick: the power-gate DIAG appears, nothing else — no crash.
#include "fw2.h"
#include "platform/diag.h"
#include "pico/stdlib.h"
#include "ff.h"
#include <string.h>
#include <strings.h>
#include <stdio.h>

static inline uint16_t be16(uint16_t c) {
    return (uint16_t)((c >> 8) | (c << 8));
}

static void status(const char *headline, const char *detail, bool ok) {
    st7796_fill_rect(8, 75, 464, 160, be16(0x0000));
    st7796_draw_text(12, 90, 2, be16(ok ? 0x07E0 : 0xF800),
                     be16(0x0000), headline);
    if (detail)
        st7796_draw_text(12, 130, 2, be16(0xFFFF), be16(0x0000), detail);
}

static unsigned count_ir_files(void) {
    DIR dir;
    FILINFO fi;
    unsigned n = 0;
    if (f_opendir(&dir, "0:/") != FR_OK) return 0;
    while (f_readdir(&dir, &fi) == FR_OK && fi.fname[0]) {
        fw2_app_recovery_task();
        size_t len = strlen(fi.fname);
        if (!(fi.fattrib & AM_DIR) && len > 3 &&
            !strcasecmp(fi.fname + len - 3, ".ir")) n++;
    }
    f_closedir(&dir);
    DIAG("hello_usbdrive: %u .ir file(s) at volume root\n", n);
    return n;
}

int main(void) {
    board_init();
    fw2_app_recovery_init();
    DIAG("hello_usbdrive: up\n");
    st7796_init();
    fw2_app_about_use_lcd();
    st7796_fill_screen(be16(0x0000));
    st7796_draw_text(12, 12, 2, be16(0xFFFF), be16(0x0000), "USB MASS STORAGE");
    st7796_draw_text(12, 45, 1, be16(0xFFFF), be16(0x0000),
                     "Insert a FAT32 USB drive");
    board_backlight_set(1);
    status("WAITING FOR DRIVE", "USB ports powered", true);
    usb_store_init();                 // ioexp_usb_pwr(true) + host stack init
    bool was_mounted = false;
    while (true) {
        fw2_app_recovery_task();
        usb_store_task();
        bool m = usb_store_mounted();
        if (m != was_mounted) {
            was_mounted = m;
            if (m) {
                unsigned n = count_ir_files();
                char detail[48];
                snprintf(detail, sizeof detail, "%u .ir file(s) in root", n);
                status("DRIVE MOUNTED", detail, true);
            } else {
                status("DRIVE REMOVED", "Waiting for a drive", false);
            }
        }
        fw2_app_recovery_sleep_ms(2);
    }
}

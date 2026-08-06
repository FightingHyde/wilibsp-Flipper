/* hello_sdcard — read and write the SD card from the display CPU over the
 * FwGUI link (UART0 @ 8 Mbaud, HW flow control on GPIO 0-3). The card is
 * owned by the MAIN CPU; ow_open_fwgui arms an SDFS client that reaches
 * fw2main's server, the same route the stock display firmware uses.
 *
 * The main CPU must run the stock FreeWili 2 firmware (it carries both the
 * OneWili bridge and the SDFS server).
 *
 * Exercises mkdir -> append (handle writes) -> whole-file read -> stat ->
 * directory listing. Watch it on RTT: `fw rtt`. */
#include "fw2.h"
#include "platform/diag.h"
#include "pico/stdlib.h"
#include "onewili.h"
#include "onewili_fwgui.h"
#include "onewili_sd.h"
#include "input/app_recovery_onewili.h"
#include <stdio.h>
#include <string.h>

#define LOG_DIR  "/owlog"
#define LOG_PATH LOG_DIR "/run.txt"

static void print_entry(const char* name, bool is_dir, uint32_t size, void* user) {
    (void)user;
    DIAG("  %-24s %s %lu\n", name, is_dir ? "<DIR>" : "     ", (unsigned long)size);
}

/* ~37 KB of link buffers — far too big for the 2 KB stack. */
static ow_device dev;
/* Read-back scratch. Ordinary apps are loaded directly into SRAM, so keep it modest. */
static char readbuf[1024];

static inline uint16_t be16(uint16_t c) {
    return (uint16_t)((c >> 8) | (c << 8));
}

static void status(const char *text, bool ok) {
    st7796_fill_rect(8, 75, 464, 190, be16(0x0000));
    for (int row = 0; *text && row < 4; row++) {
        char line[40];
        const char *end = strchr(text, '\n');
        size_t len = end ? (size_t)(end - text) : strlen(text);
        if (len >= sizeof line) len = sizeof line - 1;
        memcpy(line, text, len);
        line[len] = '\0';
        st7796_draw_text(12, 90 + row * 34, 2,
                         be16(ok ? 0x07E0 : 0xF800), be16(0x0000), line);
        text += len;
        if (*text == '\n') text++;
    }
}

int main(void) {
    board_init();   /* must precede ow_open_fwgui: uart_init reads clk_peri */
    fw2_app_recovery_init();
    st7796_init();
    st7796_fill_screen(be16(0x0000));
    st7796_draw_text(12, 12, 2, be16(0xFFFF), be16(0x0000), "SD CARD / ONEWILI");
    st7796_draw_text(12, 45, 1, be16(0xFFFF), be16(0x0000),
                     "Writes and reads /owlog/run.txt");
    board_backlight_set(1);
    status("CONNECTING TO MAIN...", true);

    /* ow_open_fwgui only sends the reset byte (no handshake), so it cannot
     * currently fail; the loop future-proofs a smarter open. A missing bridge
     * surfaces below instead, as SD timeouts. */
    while (fw2_app_recovery_open_onewili(&dev) != OW_OK) {
        fw2_app_recovery_task();
        DIAG("hello_sdcard: FwGUI link open failed (is the main CPU running stock fw?), retry in 1 s\n");
        status("MAIN LINK FAILED\nRETRYING...", false);
        fw2_app_recovery_sleep_ms(1000);
    }
    if (fw2_app_recovery_wrap_sd() != OW_OK) {
        DIAG("hello_sdcard: recovery-aware SD transport setup failed\n");
        status("SD TRANSPORT SETUP FAILED", false);
    }
    DIAG("hello_sdcard: link up, SD client armed\n");
    status("LINK OK\nWRITING 10 RECORDS...", true);

    /* Repeating mkdir is fine — an existing directory reports a clean error. */
    ow_sd_mkdir(&dev, LOG_DIR);
    fw2_app_recovery_task();

    bool writes_ok = true;
    for (int i = 0; i < 10; i++) {
        fw2_app_recovery_task();
        char line[64];
        int n = snprintf(line, sizeof line, "tick %d\n", i);
        ow_sd_file f;
        if (ow_sd_open(&dev, &f, LOG_PATH, OW_SD_APPEND) != OW_OK) {
            writes_ok = false;
            DIAG("hello_sdcard: open failed (sdfs %d)\n", (int)ow_sd_last_error());
            status("SD OPEN FAILED", false);
        } else {
            fw2_app_recovery_task();
            if (ow_sd_write(&f, line, (size_t)n) != OW_OK)
                writes_ok = false;
            fw2_app_recovery_task();
            /* Writes are fire-and-forget; close is where a dropped chunk shows up. */
            if (ow_sd_close(&f) != OW_OK) {
                writes_ok = false;
                DIAG("hello_sdcard: close failed (sdfs %d)\n", (int)ow_sd_last_error());
            }
            fw2_app_recovery_task();
        }
        fw2_app_recovery_sleep_ms(1000);
    }

    bool is_dir = false;
    uint32_t size = 0;
    char result[96];
    bool stat_ok = ow_sd_stat(&dev, LOG_PATH, &is_dir, &size) == OW_OK;
    if (stat_ok)
        DIAG("hello_sdcard: %s is %lu bytes\n", LOG_PATH, (unsigned long)size);
    else
        DIAG("hello_sdcard: stat failed (sdfs %d)\n", (int)ow_sd_last_error());
    fw2_app_recovery_task();

    size_t got = 0;
    bool read_ok = ow_sd_get_mem(&dev, LOG_PATH, readbuf, sizeof readbuf - 1, &got) == OW_OK;
    if (read_ok) {
        readbuf[got] = '\0';
        DIAG("hello_sdcard: read back %u bytes:\n%s", (unsigned)got, readbuf);
    } else {
        DIAG("hello_sdcard: read failed (sdfs %d)\n", (int)ow_sd_last_error());
    }
    fw2_app_recovery_task();

    DIAG("hello_sdcard: %s:\n", LOG_DIR);
    bool list_ok = ow_sd_list(&dev, LOG_DIR, print_entry, 0) == OW_OK;
    if (!list_ok)
        DIAG("hello_sdcard: list failed (sdfs %d)\n", (int)ow_sd_last_error());
    fw2_app_recovery_task();

    DIAG("hello_sdcard: done\n");
    snprintf(result, sizeof result, "%s\nFILE %lu BYTES\nREAD BACK %u BYTES",
             writes_ok && stat_ok && read_ok && list_ok ? "SD TEST PASSED" : "SD TEST FAILED",
             (unsigned long)size, (unsigned)got);
    status(result, writes_ok && stat_ok && read_ok && list_ok);
    for (;;) {
        fw2_app_recovery_task();
        fw2_app_recovery_sleep_ms(100);
    }
}

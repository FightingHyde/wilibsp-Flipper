// apps/hello_agentio — known-pattern app for validating agentio screen capture.
// Draws colour bars with labelled text so a captured PNG can be checked by eye
// (or by an agent) against what the code says it drew. Touch paints a marker so
// injected touches are visible in a capture too.
#include "fw2.h"
#include "platform/diag.h"
#include "platform/psram.h"    // psram_init() — not pulled in by fw2.h

// Wire-order (byte-swapped) RGB565 — the panel takes big-endian pixels.
#define BE(c) ((uint16_t)(((c) >> 8) | ((c) << 8)))

static const uint16_t k_bars[] = {
    BE(0xF800), BE(0x07E0), BE(0x001F), BE(0xFFE0),
    BE(0x07FF), BE(0xF81F), BE(0xFFFF), BE(0x0000),
};
static const char *k_labels[] = {
    "RED", "GRN", "BLU", "YEL", "CYN", "MAG", "WHT", "BLK",
};

int main(void)
{
    board_init();
    fw2_app_recovery_init();
    size_t psram_bytes = psram_init();
    if (psram_bytes < (size_t)ST7796_W * ST7796_H * 2) {
        DIAG("hello_agentio: PSRAM absent/too small (%u bytes) - halting\n",
             (unsigned)psram_bytes);
        for (;;) {
            fw2_app_recovery_task();
            tight_loop_contents();
        }
    }
    st7796_init();
    ft6336_init();
    board_backlight_set(1);

    // agentio_init() zeroes the shadow framebuffer, so it must run before
    // anything we want a capture to see is drawn — otherwise the pattern
    // below is erased from the shadow the instant it starts (see
    // docs/drivers/agentio.md, "three app calls").
    fw2kb_t kb;
    fw2kb_init(&kb);
    agentio_init();
    agentio_bind_keyboard(&kb);

    const int n = (int)(sizeof k_bars / sizeof k_bars[0]);
    const int bar_w = ST7796_W / n;
    for (int i = 0; i < n; i++) {
        st7796_fill_rect(i * bar_w, 0, bar_w, 200, k_bars[i]);
        st7796_draw_text(i * bar_w + 4, 208, 1, BE(0xFFFF), BE(0x0000),
                         k_labels[i]);
    }
    st7796_draw_text(8, 240, 2, BE(0xFFFF), BE(0x0000), "hello_agentio");
    st7796_draw_text(8, 268, 1, BE(0x07E0), BE(0x0000),
                     "fw screenshot -o shot.png");
    DIAG("hello_agentio: pattern drawn, agentio up\n");

    for (;;) {
        fw2_app_recovery_task();

        uartkbd_event_t bev;
        while (uartkbd_next_event(&bev)) {
            if (!bev.pressed) continue;
            DIAG("hello_agentio: btn %u\n", (unsigned)bev.btn);
            // Paint a swatch so an injected press shows up in a capture.
            st7796_fill_rect(8 + 20 * (int)bev.btn, 296, 16, 16, BE(0x07E0));
        }

        uint16_t tx, ty;
        if (ft6336_poll(&tx, &ty))
            st7796_fill_rect((int)tx - 4, (int)ty - 4, 8, 8, BE(0xF81F));

        agentio_task();
    }
}

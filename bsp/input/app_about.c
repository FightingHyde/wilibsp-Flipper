#include "input/app_about.h"

#include "common/uf2_info.h"
#include "display/st7796.h"
#include "input/uartkbd.h"
#include "pico/stdlib.h"
#include <stdio.h>

#define ABOUT_HOLD_MS 5000u
#define FRAME_MAX_AGE_MS 1100u
#define BE16(c) ((uint16_t)(((c) >> 8) | ((c) << 8)))

extern const unsigned char fw2app_uf2_info[];
extern const char fw2app_repository_url[];

static fw2_app_about_renderer_t s_renderer;
static bool s_tracking;
static bool s_shown;
static uint32_t s_pressed_at;

static void lcd_renderer(const char *name, uint16_t version,
                         const char *repository) {
    char line[48];
    st7796_fill_screen(BE16(0x0000));
    st7796_draw_text(12, 20, 3, BE16(0xFFFF), BE16(0x0000), "ABOUT");
    st7796_draw_text(12, 70, 2, BE16(0x07E0), BE16(0x0000), name);
    snprintf(line, sizeof line, "VERSION %03u", (unsigned)version);
    st7796_draw_text(12, 105, 2, BE16(0xFFFF), BE16(0x0000), line);
    st7796_draw_text(12, 155, 1, BE16(0x07FF), BE16(0x0000), "SOURCE:");
    st7796_draw_text(12, 180, 1, BE16(0xFFFF), BE16(0x0000), repository);
    st7796_draw_text(12, 285, 1, BE16(0xFFFF), BE16(0x0000),
                     "RELEASE PAGE TO RETURN");
}

void fw2_app_about_set_renderer(fw2_app_about_renderer_t renderer) {
    s_renderer = renderer;
}

void fw2_app_about_use_lcd(void) {
    fw2_app_about_set_renderer(lcd_renderer);
}

void fw2_app_about_task(void) {
    bool down = uartkbd_button_down_fresh(UARTKBD_BTN_PAGE,
                                          FRAME_MAX_AGE_MS);
    uint32_t now = (uint32_t)(time_us_64() / 1000u);
    if (!down) {
        s_tracking = false;
        s_shown = false;
        return;
    }
    if (!s_tracking) {
        s_tracking = true;
        s_pressed_at = now;
        return;
    }
    if (s_shown || !s_renderer ||
        (uint32_t)(now - s_pressed_at) < ABOUT_HOLD_MS)
        return;
    s_shown = true;
    const fw2app_uf2_info_t *info =
        (const fw2app_uf2_info_t *)fw2app_uf2_info;
    s_renderer(info->name, info->app_version, fw2app_repository_url);
}

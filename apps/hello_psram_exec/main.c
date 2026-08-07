#include "fw2.h"
#include "pico/stdlib.h"
#include "platform/diag.h"

static volatile uint32_t s_psram_heartbeat;

int main(void) {
    DIAG("psram: main\n");
    DIAG("psram: board ready\n");
    fw2_app_recovery_init();
    DIAG("psram: recovery ready\n");
    for (unsigned i = 0; i < 300u; ++i) {
        fw2_app_recovery_task();
        busy_wait_us_32(10000);
    }
    uint32_t rails = 0;
    bool rails_valid = picpwr_rails(&rails);
    DIAG("psram: power frames=%u rails=%x valid=%u\n",
         uartkbd_frames(), rails, rails_valid);
    DIAG("psram: cycling RGB LED rail\n");
    if (!picpwr_cycle(picpwr_zone_bit(PICPWR_ZONE_RGB_LEDS)))
        DIAG("psram: RGB LED rail cycle unavailable\n");
    else
        DIAG("psram: RGB LED rail cycled\n");
    st7796_init();
    DIAG("psram: lcd ready\n");
    fw2_app_about_use_lcd();
    agentio_init();
    DIAG("psram: agentio ready\n");
    st7796_fill_screen(0x0000);
    st7796_draw_text(28, 70, 3, 0xFFFF, 0x0000, "EXECUTING FROM PSRAM");
    st7796_draw_text(70, 135, 2, 0x07E0, 0x0000, "NORMAL BSP + SDK RUNTIME");
    st7796_draw_text(80, 205, 2, 0xFFE0, 0x0000, "HOLD HOME TO RETURN");
    board_backlight_set(1);
    DIAG("psram: ui ready\n");
    for (;;) {
        fw2_app_recovery_task();
        agentio_task();
        ++s_psram_heartbeat;
        fw2_app_recovery_sleep_ms(10);
    }
}

#include "fw2.h"
#include "pico/stdlib.h"
#include "hardware/timer.h"

static repeating_timer_t flipper_timer;

static bool flipper_timer_callback(repeating_timer_t *rt) {
    (void)rt;
    // Periodic work here (runs every 1000ms)
    return true; // Keep repeating
}

int main(void) {
    board_init();
    fw2_app_recovery_init();
    st7796_init();
    fw2_app_about_use_lcd();
    st7796_fill_screen(0x0000);
    board_backlight_set(1);
    
    // Start a 1-second repeating timer
    add_repeating_timer_ms(1000, flipper_timer_callback, NULL, &flipper_timer);
    
    st7796_draw_text(8, 8, 2, 0xFFFF, 0x0000, "Flipper Zero App");
    
    // Main loop
    for (;;) {
        fw2_app_recovery_task();
        
        // Check for home button to quit
        // Note: fw2_input_pressed and fw2_input_held may not exist in this BSP
        // You'll need to check what input functions are available
        
        tight_loop_contents();
    }
}

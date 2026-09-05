#include "fw2.h"
#include "pico/stdlib.h"

static repeating_timer_t flipper_timer;

static bool flipper_timer_callback(repeating_timer_t *rt) {
    (void)rt;
    // Periodic work here (runs every 1000ms)
    return true; // Keep repeating
}

int flipper_app_main(fw2_app_ctx_t* ctx) {
    (void)ctx;
    // Start a 1-second repeating timer instead of a FreeRTOS task
    add_repeating_timer_ms(1000, flipper_timer_callback, NULL, &flipper_timer);
    return 0;
}

void flipper_app_tick(fw2_app_ctx_t* ctx) {
    if(fw2_input_pressed(FW2_BTN_HOME) && fw2_input_held(FW2_BTN_HOME, 1000))
        fw2_app_quit(ctx);
}

void flipper_app_exit(void) {
    cancel_repeating_timer(&flipper_timer);
}

FW2_APP_REGISTER(
    .name = "Flipper Zero",
    .version = "0.1.0",
    .author = "Community",
    .main = flipper_app_main,
    .tick = flipper_app_tick,
    .exit = flipper_app_exit,
);

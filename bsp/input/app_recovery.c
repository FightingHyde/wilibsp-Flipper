#include "input/app_recovery.h"

#ifndef HOST_TEST
#include "hardware/watchdog.h"
#include "pico/time.h"
#include "input/uartkbd.h"
#include "platform/diag.h"
#endif

void fw2_app_recovery_state_init(fw2_app_recovery_state_t *state) {
    state->pressed_at_ms = 0;
    state->tracking = false;
    state->fired = false;
}

bool fw2_app_recovery_state_update(fw2_app_recovery_state_t *state,
                                   bool home_down, uint32_t now_ms) {
    if (!home_down) {
        state->tracking = false;
        state->fired = false;
        return false;
    }
    if (!state->tracking) {
        state->pressed_at_ms = now_ms;
        state->tracking = true;
        return false;
    }
    if (!state->fired &&
        (uint32_t)(now_ms - state->pressed_at_ms) >= FW2_APP_RECOVERY_HOLD_MS) {
        state->fired = true;
        return true;
    }
    return false;
}

#ifndef HOST_TEST
static fw2_app_recovery_state_t recovery;

void fw2_app_recovery_init(void) {
    uartkbd_init();
    fw2_app_recovery_state_init(&recovery);
}

void fw2_app_recovery_task(void) {
    uartkbd_task();
    const bool home = (uartkbd_buttons() & (1u << UARTKBD_BTN_HOME)) != 0;
    if (fw2_app_recovery_state_update(&recovery, home,
                                      (uint32_t)(time_us_64() / 1000u))) {
        DIAG("app: HOME held, rebooting to recovery loader\n");
        watchdog_reboot(0, 0, 0);
        while (true) tight_loop_contents();
    }
}
#endif

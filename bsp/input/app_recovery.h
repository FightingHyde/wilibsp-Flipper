#ifndef FW2_APP_RECOVERY_H
#define FW2_APP_RECOVERY_H

#include <stdbool.h>
#include <stdint.h>

#define FW2_APP_RECOVERY_HOLD_MS 5000u

typedef struct {
    uint32_t pressed_at_ms;
    bool tracking;
    bool fired;
} fw2_app_recovery_state_t;

void fw2_app_recovery_state_init(fw2_app_recovery_state_t *state);
bool fw2_app_recovery_state_update(fw2_app_recovery_state_t *state,
                                   bool home_down, uint32_t now_ms);

/* Initializes the keyboard link and recovery state. Call once after board_init. */
void fw2_app_recovery_init(void);

/* Services keyboard input and normally reboots after HOME is held for 5 s. */
void fw2_app_recovery_task(void);

#endif

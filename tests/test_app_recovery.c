#include <assert.h>
#include <stdint.h>
#include "input/app_recovery.h"

int main(void) {
    fw2_app_recovery_state_t state;
    fw2_app_recovery_state_init(&state);
    assert(!fw2_app_recovery_state_update(&state, false, 100));
    assert(!fw2_app_recovery_state_update(&state, true, 100));
    assert(!fw2_app_recovery_state_update(&state, true, 5099));
    assert(fw2_app_recovery_state_update(&state, true, 5100));
    assert(!fw2_app_recovery_state_update(&state, true, 9000));
    assert(!fw2_app_recovery_state_update(&state, false, 9001));

    /* A stale/lost input link is presented as released and cancels a hold. */
    fw2_app_recovery_state_init(&state);
    assert(!fw2_app_recovery_state_update(&state, true, 100));
    assert(!fw2_app_recovery_state_update(&state, false, 1201));
    assert(!fw2_app_recovery_state_update(&state, true, 5100));

    /* Unsigned elapsed time deliberately supports a millisecond wrap. */
    fw2_app_recovery_state_init(&state);
    assert(!fw2_app_recovery_state_update(&state, true, UINT32_MAX - 1000u));
    assert(fw2_app_recovery_state_update(&state, true, 3999u));
    return 0;
}

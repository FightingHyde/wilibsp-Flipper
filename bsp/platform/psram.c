// src/platform/psram.c — thin shim over the SDK's hardware_psram (SDK 2.3.0+).
//
// The 130-line hand-rolled APS6404L bring-up this replaced (harvested from
// evaderkrub/usbcamfw) is gone: the SDK now owns QMI setup, sizing and timing.
// PSRAM comes up during runtime_init, before main(), driven by PICO_PSRAM_CS_PIN
// and PICO_PSRAM_SIZE_BYTES in bsp/boards/freewili2.h; board_init_clk() re-times
// it for the overclock. So by the time an app runs, PSRAM is already mapped at
// PSRAM_BASE and psram_init() has nothing left to do but report the size.
#include "platform/psram.h"
#include "hardware/psram.h"

size_t psram_init(void) {
    return psram_is_available() ? psram_get_size() : 0;
}

// End of the linker's PSRAM allocation (sections_psram.incl). Everything below it
// belongs to __in_psram / __uninitialized_psram variables; the selftest starts
// above it so running it can never corrupt live data.
extern char __psram_end__;

int psram_selftest(size_t test_bytes) {
    size_t size = psram_init();
    if (size == 0) return 0;
    uintptr_t start = (uintptr_t)&__psram_end__;
    uintptr_t end   = (uintptr_t)PSRAM_BASE + size;
    if (start + test_bytes > end) {
        if (start >= end) return 0;          // PSRAM fully allocated — nothing free
        test_bytes = end - start;            // test whatever is left
    }
    volatile uint32_t *p = (volatile uint32_t *)start;
    size_t n = test_bytes / 4;
    for (size_t i = 0; i < n; i++) p[i] = (uint32_t)(i * 2654435761u);
    for (size_t i = 0; i < n; i++)
        if (p[i] != (uint32_t)(i * 2654435761u)) return 0;
    return 1;
}

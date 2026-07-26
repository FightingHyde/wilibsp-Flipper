// src/platform/psram.h — APS6404L (8 MB) on the RP2350 QMI/XIP M1 window.
// Thin shim over the SDK's hardware_psram (SDK 2.3.0+). PSRAM is brought up and
// memory-mapped at PSRAM_BASE during runtime_init, before main(), from
// PICO_PSRAM_CS_PIN / PICO_PSRAM_SIZE_BYTES in bsp/boards/freewili2.h;
// board_init_clk() re-times it for the overclock.
#ifndef PSRAM_H
#define PSRAM_H
#include <stddef.h>

#define PSRAM_BASE 0x11000000u   // M1 (XIP CS1) memory-mapped base

// Place large buffers in PSRAM with the SDK's section macros rather than casting
// PSRAM_BASE — the linker's PSRAM region ALSO starts at 0x11000000, so a raw
// PSRAM_BASE pointer silently aliases whatever the linker allocated there:
//
//     static uint16_t __uninitialized_psram("fb") s_fb[480 * 320];
//
// Returns the PSRAM size in bytes, or 0 if none is present. Safe to call at any
// point after board_init(); it only reports what runtime_init already did.
size_t psram_init(void);

// Walking-value RAM test over the FREE PSRAM above the linker's allocation (so it
// cannot corrupt __in_psram / __uninitialized_psram variables). test_bytes is
// clamped to what is actually free. Returns 1 on pass, 0 on mismatch or when no
// PSRAM (or no free PSRAM) is available.
int psram_selftest(size_t test_bytes);

#endif

// src/input/ft6336.h — FT6336U capacitive touch on I2C1, polled (no INT; RST shares
// the panel HW reset). Mirrors the proven freewili reference driver
// (rmpLib/rpCAPTouchFT6336): single 6-byte status read, DEVICE_MODE set at init, and
// coordinates returned already oriented to the 480x320 landscape panel.
#ifndef FT6336_H
#define FT6336_H
#include <stdint.h>
#include <stdbool.h>

// The I2C bus must already be up (board_i2c1_init brings up i2c1 @ 400 kHz). Reads the
// chip-ID to confirm presence and writes DEVICE_MODE=normal. Returns true on ACK.
bool ft6336_init(void);

// Poll for a single touch. On exactly one finger down, writes SCREEN coordinates
// (x: 0..479, y: 0..319 — already oriented to the panel, matching the reference driver)
// and returns true; returns false when nothing (or more than one point) is touched.
bool ft6336_poll(uint16_t* x, uint16_t* y);

// Poll for up to TWO touch points (the FT6336U's hardware maximum). Returns
// the point count 0..2 and fills (x1,y1) — and (x2,y2) when two are down —
// oriented to the panel like ft6336_poll. NOTE: register slot order follows
// the chip's touch IDs, so after one of two fingers lifts, the survivor may
// appear in either slot — callers should track points as a SET, not by slot.
// An injected point reports as a single touch.
int ft6336_poll2(uint16_t* x1, uint16_t* y1, uint16_t* x2, uint16_t* y2);

// agentio touch injection. While `down` is set, ft6336_poll() returns the
// injected point and skips the I2C read entirely, so injection works with no
// finger (and no touch controller) present. ft6336_inject_reads() counts polls
// that observed the injected point — agentio uses it to hold a tap until the
// app has actually seen it at least once.
void     ft6336_inject_set(uint16_t x, uint16_t y, bool down);
uint32_t ft6336_inject_reads(void);

// Link-health counters. Errors are failed bounded I2C transactions;
// recoveries are controller/bus reinitializations after repeated failures.
uint32_t ft6336_i2c_errors(void);
uint32_t ft6336_i2c_recoveries(void);

#endif // FT6336_H

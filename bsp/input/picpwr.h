/*
 * picpwr — power-zone control over the coprocessor link (uartkbd's UART).
 * The coprocessor sequences every switched rail on the board; rails
 * outside its boot subset stay off until an application requests them
 * (the audio codec and CAN rails are the common casualties). Protocol:
 * docs/drivers/power.md. Polled/foreground like the rest of this BSP;
 * uartkbd_init() must have run first.
 */
#ifndef PICPWR_H
#define PICPWR_H

#include <stdint.h>
#include <stdbool.h>
#include "picpwr_frame.h"

/* Send a complete power configuration. All four fields go in one atomic
 * write — the device has no partial form and no readback of the sleep or
 * wake fields, so callers must always provide complete values. Any zone
 * bit left clear in awake switches that rail OFF. Only zones 1..17 are
 * expressible: mask bits above that are reserved-zero and this API
 * clamps them out of every frame it sends (docs/drivers/power.md).
 * Rate-limited: returns false without sending
 * while a previous command's rail walk (~1 s, blocking on the device
 * side) may still be running — retry later. The cfg is cached and seeds
 * picpwr_ensure_awake(). */
bool picpwr_send(const picpwr_cfg_t *cfg);

/* Live rail state decoded from the last status frame
 * (zones 1..17 -> bits 0..16, high = powered). False until a status
 * frame has been parsed. This is the only trustworthy source of rail
 * state — other actors move rails without notice, so never trust a
 * local cache for readback. */
bool picpwr_rails(uint32_t *rails_out);

/* Convenience: ensure the given rails are enabled, additively — no rail
 * that currently reads as powered is ever cleared. Seeds the awake mask
 * from LIVE rail state (never the cache), requiring two agreeing reads so
 * an unsettled first frame cannot drop rails it failed to report; ORs in
 * zone_bits; carries the cached sleep/wake fields. Comparisons and the
 * sent mask cover zones 1..17 only — reserved bits are always zero.
 * Returns true immediately when the rails already read as on; false when
 * stable live state is unavailable or the rate limit is active (retry
 * later). */
bool picpwr_ensure_awake(uint32_t zone_bits);

/* Like picpwr_ensure_awake(), and additionally remembers the rails so
 * picpwr_task() re-asserts them if they later read as off (the device
 * changes rails on its own: sleep/wake, USB attach/detach, watchdog).
 * The primary entry point for applications: request the rails your
 * peripherals need once at startup, poll picpwr_task() from the main
 * loop, and the driver handles the rest. */
bool picpwr_keep_awake(uint32_t zone_bits);

/* Power-cycle selected rails while preserving every other live rail. If a
 * selected rail is already off it remains off. Blocks through the two
 * sequencer walks while continuing to service app recovery/status input. */
bool picpwr_cycle(uint32_t zone_bits);

/* Poll from the application's main loop (cheap; acts at most once per
 * received status frame). Re-asserts rails registered with
 * picpwr_keep_awake() when they read as off — debounced over two
 * consecutive status frames and spaced by the send rate limit, so a
 * single stale snapshot or an in-progress apply never causes a storm. */
void picpwr_task(void);

#endif /* PICPWR_H */

/*
 * uartkbd — FW2 coprocessor link binding: UART1 @ 62500 8N1 on GPIO38
 * (TX) / GPIO39 (RX). Status/button frames arrive unsolicited; command
 * frames go out over the same pair via uartkbd_cmd_send(). Polled — call
 * uartkbd_task() every main-loop iteration.
 */
#ifndef UARTKBD_H
#define UARTKBD_H

#include <stddef.h>
#include "uartkbd_parse.h"

void     uartkbd_init(void);
void     uartkbd_task(void);
bool     uartkbd_next_event(uartkbd_event_t *ev);
uint16_t uartkbd_buttons(void);
uint8_t  uartkbd_flags(void);
bool     uartkbd_charger(uartkbd_charger_t *out);
uint32_t uartkbd_frames(void);
uint32_t uartkbd_errors(void);

/* Copy of the last valid status frame's 20-byte payload (false until one
 * has arrived). Rail state decodes from it via picpwr_rails_decode(). */
bool     uartkbd_status_raw(uint8_t out[20]);

/* Send one command frame to the coprocessor over the same link. The
 * receiver is command-gated: a UART break wakes it, a bare activity byte
 * (0xC9) signals it is listening, then the frame goes out. Blocks up to
 * ~560 ms worst case. Returns true when the activity byte was seen;
 * false means the frame was sent after the wait timed out. Inbound bytes
 * consumed while waiting are discarded and the frame parser resyncs.
 * Callers must serialize sends and respect the device's post-command
 * settling time (docs/drivers/power.md). */
bool     uartkbd_cmd_send(const uint8_t *frame, size_t len);

/* agentio: set the injected button mask (bit N = uartkbd_btn_t N held).
 * Works with or without a keyboard attached — uartkbd_init() primes the
 * parser immediately, and each call here makes uartkbd_task() synthesize one
 * well-formed idle frame on its next pass (never on a timer), so the edge
 * fires promptly without disturbing a real keyboard's own frames. */
void uartkbd_inject_set(uint16_t mask);

#endif /* UARTKBD_H */

/*
 * uartkbd — FW2 UART keyboard binding: UART1 @ 62500 8N1 on GPIO38 (TX,
 * claimed but never driven) / GPIO39 (RX). Frames arrive unsolicited;
 * RX-only. Polled — call uartkbd_task() every main-loop iteration.
 */
#ifndef UARTKBD_H
#define UARTKBD_H

#include "uartkbd_parse.h"

void     uartkbd_init(void);
void     uartkbd_task(void);
bool     uartkbd_next_event(uartkbd_event_t *ev);
uint16_t uartkbd_buttons(void);
uint8_t  uartkbd_flags(void);
bool     uartkbd_charger(uartkbd_charger_t *out);
uint32_t uartkbd_frames(void);
uint32_t uartkbd_errors(void);

/* agentio: set the injected button mask (bit N = uartkbd_btn_t N held).
 * Works with or without a keyboard attached — uartkbd_task() synthesizes a
 * well-formed idle frame whenever the wire has been silent for 50 ms, so the
 * parser stays primed and injected edges always fire. */
void uartkbd_inject_set(uint16_t mask);

#endif /* UARTKBD_H */

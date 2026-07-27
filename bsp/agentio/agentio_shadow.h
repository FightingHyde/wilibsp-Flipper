/*
 * agentio_shadow — pure window-walking for the agentio shadow framebuffer.
 *
 * The ST7796 is write-only on this board (SPI1 RX is wired as the LCD's DC
 * output), so capture reads a shadow the display driver mirrors into. This
 * module owns the address arithmetic: given the panel's current GRAM window
 * and the raw byte stream being pushed to it, it writes those bytes into a
 * plain framebuffer at the right coordinates.
 *
 * Bytes are stored VERBATIM in wire order (MSB of each RGB565 pixel first),
 * so this file needs no endianness knowledge at all.
 *
 * Pure: no hardware includes, host-tested by tests/test_agentio_shadow.c.
 */
#ifndef AGENTIO_SHADOW_H
#define AGENTIO_SHADOW_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t *fb;            /* w * h * 2 bytes, wire order */
    int      w, h;
    int      x0, y0, x1, y1;/* current window, inclusive */
    int      cx, cy;        /* cursor, absolute panel coords */
    uint8_t  pending;       /* first byte of a pixel split across calls */
    bool     has_pending;
} agentio_shadow_t;

/* fb must be at least w*h*2 bytes. Leaves the window empty (no pixels are
 * accepted until agentio_shadow_set_window). */
void agentio_shadow_init(agentio_shadow_t *s, uint8_t *fb, int w, int h);

/* Mirrors CASET/RASET. Resets the cursor to (x0,y0) and drops any half-written
 * pixel. Windows may extend past the framebuffer; out-of-bounds pixels are
 * consumed and discarded so the cursor stays in step with the panel. */
void agentio_shadow_set_window(agentio_shadow_t *s, int x0, int y0,
                               int x1, int y1);

/* Feed the same bytes that go to the panel. Pixels past the end of the window
 * are discarded (the panel would wrap; the shadow deliberately does not). */
void agentio_shadow_write(agentio_shadow_t *s, const uint8_t *bytes, size_t n);

#endif /* AGENTIO_SHADOW_H */

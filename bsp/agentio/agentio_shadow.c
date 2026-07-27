#include "agentio/agentio_shadow.h"

void agentio_shadow_init(agentio_shadow_t *s, uint8_t *fb, int w, int h)
{
    s->fb = fb;
    s->w = w;
    s->h = h;
    /* An empty window: x1 < x0 means store_pixel accepts nothing. */
    s->x0 = s->y0 = 0;
    s->x1 = s->y1 = -1;
    s->cx = s->cy = 0;
    s->pending = 0;
    s->has_pending = false;
}

void agentio_shadow_set_window(agentio_shadow_t *s, int x0, int y0,
                               int x1, int y1)
{
    s->x0 = x0; s->y0 = y0; s->x1 = x1; s->y1 = y1;
    s->cx = x0; s->cy = y0;
    s->has_pending = false;
}

/* Store one pixel at the cursor (if it lands inside the framebuffer) and
 * advance the cursor within the window. */
static void store_pixel(agentio_shadow_t *s, uint8_t hi, uint8_t lo)
{
    /* Looks redundant with the cx<w/cy<h bounds check below — it is not.
     * It is what keeps a call safe *before a window has ever been set*:
     * right after agentio_shadow_init() (fb pointing at a real, sized
     * buffer) but before the first CASET/RASET, x1 == y1 == -1 (the "empty
     * window" sentinel set in agentio_shadow_init) and this guard rejects
     * every pixel outright, even though cx/cy=0 would otherwise pass the
     * in-bounds check below and land a stray pixel at (0,0). For a
     * genuinely zero-initialized (never-inited) static instance, fb is
     * NULL and w/h are also 0, so the bounds check alone happens to save
     * it today — but do not delete this line as "dead code": it is the
     * only thing standing between the empty-window state and a write
     * through fb. */
    if (s->cy > s->y1 || s->cx > s->x1) return;   /* window exhausted */

    if (s->cx >= 0 && s->cy >= 0 && s->cx < s->w && s->cy < s->h) {
        uint8_t *p = &s->fb[((size_t)s->cy * (size_t)s->w + (size_t)s->cx) * 2];
        p[0] = hi;
        p[1] = lo;
    }
    if (++s->cx > s->x1) {                        /* wrap within the window */
        s->cx = s->x0;
        s->cy++;
    }
}

void agentio_shadow_write(agentio_shadow_t *s, const uint8_t *bytes, size_t n)
{
    size_t i = 0;

    if (s->has_pending && n > 0) {
        s->has_pending = false;
        store_pixel(s, s->pending, bytes[0]);
        i = 1;
    }
    for (; i + 1 < n; i += 2) store_pixel(s, bytes[i], bytes[i + 1]);
    if (i < n) {
        s->pending = bytes[i];
        s->has_pending = true;
    }
}

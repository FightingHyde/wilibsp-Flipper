/* picpwr — see picpwr.h. */
#include "picpwr.h"
#include "uartkbd.h"
#include "pico/stdlib.h"

/* One command per rail walk: the device applies a mask as a staggered
 * walk (~1 s) and does not parse the link while walking. Commands are
 * applied unconditionally even when nothing changed, so every send costs
 * a walk — space them out and never stack retries. */
#define PICPWR_SPACING_MS 1500u

static picpwr_cfg_t    s_cache;
static absolute_time_t s_last_send;
static bool            s_sent_any;

bool picpwr_rails(uint32_t *rails_out)
{
    uint8_t payload[20];
    if (!uartkbd_status_raw(payload)) return false;
    *rails_out = picpwr_rails_decode(payload);
    return true;
}

/* Rail state accepted only when two DISTINCT status frames agree —
 * gated on the frame counter, not wall time, because frames arrive
 * about once a second and two reads of the same cached frame always
 * "agree". A single stale or unsettled snapshot echoed back into an
 * awake mask would switch off every rail it failed to report, so one
 * frame is never trusted alone. */
static bool rails_stable(uint32_t *rails_out)
{
    uint32_t a, b;
    if (!picpwr_rails(&a)) return false;
    uint32_t seen = uartkbd_frames();
    absolute_time_t until = make_timeout_time_ms(2500);
    while (uartkbd_frames() == seen) {           /* wait for a fresh frame */
        if (time_reached(until)) return false;
        uartkbd_task();
        sleep_ms(10);
    }
    if (!picpwr_rails(&b)) return false;
    if (b != a) return false;
    *rails_out = a;
    return true;
}

bool picpwr_send(const picpwr_cfg_t *cfg)
{
    if (s_sent_any &&
        absolute_time_diff_us(s_last_send, get_absolute_time())
            < (int64_t)PICPWR_SPACING_MS * 1000)
        return false;
    /* Clamp to the rail range no matter what the caller passed: the
     * reserved bits are not part of this API's contract in either
     * direction. One of the status indications for those lines also
     * reads back high while the correct commanded value is 0, so any
     * requested-vs-actual comparison must use the same clamp. */
    picpwr_cfg_t clamped = *cfg;
    clamped.awake &= PICPWR_ZONE_MASK_ALL;
    clamped.sleep &= PICPWR_ZONE_MASK_ALL;
    uint8_t frame[PICPWR_FRAME_LEN];
    picpwr_frame_build(frame, &clamped);
    (void)uartkbd_cmd_send(frame, sizeof frame);
    s_cache = clamped;
    s_last_send = get_absolute_time();
    s_sent_any = true;
    return true;
}

bool picpwr_ensure_awake(uint32_t zone_bits)
{
    zone_bits &= PICPWR_ZONE_MASK_ALL;
    uint32_t rails;
    if (!rails_stable(&rails)) return false;
    rails &= PICPWR_ZONE_MASK_ALL;
    if ((rails & zone_bits) == zone_bits) return true;
    picpwr_cfg_t cfg = s_cache;
    /* Rails come from live state ORed with the request — strictly
     * additive, so no rail that currently reads as powered is ever
     * cleared by this helper. picpwr_send() clamps to zones 1..17. */
    cfg.awake = rails | zone_bits;
    return picpwr_send(&cfg);
}

static uint32_t s_desired;

bool picpwr_keep_awake(uint32_t zone_bits)
{
    zone_bits &= PICPWR_ZONE_MASK_ALL;
    s_desired |= zone_bits;
    return picpwr_ensure_awake(zone_bits);
}

void picpwr_task(void)
{
    if (!s_desired) return;
    static uint32_t last_frame;
    static uint32_t prev_rails;
    static bool     missing;
    uint32_t f = uartkbd_frames();
    if (f == last_frame) return;         /* evaluate each frame once */
    last_frame = f;
    uint32_t rails;
    if (!picpwr_rails(&rails)) return;
    rails &= PICPWR_ZONE_MASK_ALL;
    if ((rails & s_desired) == s_desired) {
        missing = false;
        return;
    }
    /* A kept rail reads off. Debounce: act only when two consecutive
     * frames agree on the same rail state, so one stale or unsettled
     * snapshot never triggers a send. The send rate limit additionally
     * spaces re-asserts past the device's apply time. */
    if (!missing || rails != prev_rails) {
        missing = true;
        prev_rails = rails;
        return;
    }
    picpwr_cfg_t cfg = s_cache;
    cfg.awake = rails | s_desired;
    if (picpwr_send(&cfg)) missing = false;
}

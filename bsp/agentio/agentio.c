#include "agentio/agentio.h"

#if FW2_AGENTIO

#include <stdlib.h>
#include <string.h>
#include "agentio/agentio_proto.h"
#include "agentio/agentio_rle.h"
#include "agentio/agentio_shadow.h"
#include "display/st7796.h"
#include "input/ft6336.h"
#include "input/uartkbd.h"
#include "platform/diag.h"
#include "pico/stdlib.h"    /* __uninitialized_psram, via pico/platform/sections.h */
#include "SEGGER_RTT.h"

/* Shadow framebuffer: 480x320 wire-order RGB565 = 307,200 bytes. Placed by the
 * linker in PSRAM — never by casting PSRAM_BASE, which the linker's PSRAM
 * region also starts at. */
static uint8_t __uninitialized_psram("agentio")
    s_shadow[(size_t)ST7796_W * ST7796_H * 2];
static agentio_shadow_t s_shadow_state;

/* RTT channel 1. The up buffer blocks when full: a capture must not silently
 * drop pixels, and blocking the app loop during capture is the accepted trade
 * (see the spec's Coherence section). */
static uint8_t s_up_buf[4096];
static uint8_t s_down_buf[256];

static char   s_line[AGENTIO_CMD_MAX];
static size_t s_line_len;

static fw2kb_t *s_kb;

/* Button press queue: each entry is an inject mask applied on one
 * agentio_task() call. A press is {mask, 0} — two entries, two loop
 * iterations, so the app cannot miss the edge. */
#define AGENTIO_QUEUE_MAX 32          /* a chord is up to 10 presses = 20 entries */
static uint16_t s_queue[AGENTIO_QUEUE_MAX];
static int      s_q_head, s_q_count;

/* TYPE state: expanded lazily, one character per drain, so each chord is
 * resolved against the keyboard's real current page. */
static char s_type[AGENTIO_TYPE_MAX + 1];
static int  s_type_pos;

/* Tap state: released once the app has actually observed the point AND the
 * hold time has elapsed. */
#define AGENTIO_TAP_MS 60u
static bool     s_tap_active;
static uint32_t s_tap_deadline;
static uint32_t s_tap_reads0;

static bool queue_push(uint16_t mask)
{
    if (s_q_count >= AGENTIO_QUEUE_MAX) return false;
    s_queue[(s_q_head + s_q_count) % AGENTIO_QUEUE_MAX] = mask;
    s_q_count++;
    return true;
}

/* Queue a press: mask on one task call, released on the next. */
static bool queue_press(int btn)
{
    if (btn < 0 || btn >= UARTKBD_BTN_COUNT) return false;
    if (s_q_count + 2 > AGENTIO_QUEUE_MAX) return false;
    queue_push((uint16_t)(1u << btn));
    queue_push(0);
    return true;
}

/* Map a chord-engine button to its uartkbd button index. hello_keyboard feeds
 * UARTKBD_BTN_GREY..RED straight through as FW2KB_BTN_GRAY..RED, and maps
 * UARTKBD_BTN_PAGE to FW2KB_BTN_AI. */
static int fw2kb_btn_to_uartkbd(fw2kb_btn b)
{
    if (b >= FW2KB_BTN_GRAY && b <= FW2KB_BTN_RED) return (int)b;
    if (b == FW2KB_BTN_AI || b == FW2KB_BTN_CTRL_DOWN) return UARTKBD_BTN_PAGE;
    return -1;
}

static void reply(const char *s)
{
    SEGGER_RTT_Write(AGENTIO_RTT_CHANNEL, s, (unsigned)strlen(s));
}

/* Split `line` on single spaces into up to `max` tokens. Returns the count. */
static int split(char *line, char **argv, int max)
{
    int argc = 0;
    char *p = line;
    while (*p && argc < max) {
        while (*p == ' ') p++;
        if (!*p) break;
        argv[argc++] = p;
        while (*p && *p != ' ') p++;
        if (*p) *p++ = 0;
    }
    return argc;
}

static void dispatch(char *line)
{
    char *argv[8];
    int argc = split(line, argv, 8);
    if (argc == 0) return;

    if (strcmp(argv[0], AGENTIO_CMD_PING) == 0) {
        reply("OK\n");
        return;
    }

    if (strcmp(argv[0], AGENTIO_CMD_BTN) == 0 && argc == 2) {
        uartkbd_inject_set((uint16_t)strtoul(argv[1], 0, 16));
        reply("OK\n");
        return;
    }

    if (strcmp(argv[0], AGENTIO_CMD_TAP) == 0 && argc == 2) {
        reply(queue_press((int)strtol(argv[1], 0, 10)) ? "OK\n" : "ERR queue\n");
        return;
    }

    if (strcmp(argv[0], AGENTIO_CMD_TCH) == 0 && argc == 4) {
        uint16_t x = (uint16_t)strtoul(argv[1], 0, 10);
        uint16_t y = (uint16_t)strtoul(argv[2], 0, 10);
        int mode = (int)strtol(argv[3], 0, 10);
        if (mode == AGENTIO_TOUCH_UP) {
            ft6336_inject_set(0, 0, false);
            s_tap_active = false;
        } else if (mode == AGENTIO_TOUCH_DOWN) {
            ft6336_inject_set(x, y, true);
            s_tap_active = false;
        } else if (mode == AGENTIO_TOUCH_TAP) {
            ft6336_inject_set(x, y, true);
            s_tap_reads0 = 0;
            s_tap_deadline = to_ms_since_boot(get_absolute_time()) + AGENTIO_TAP_MS;
            s_tap_active = true;
        } else {
            reply("ERR mode\n");
            return;
        }
        reply("OK\n");
        return;
    }

    if (strcmp(argv[0], AGENTIO_CMD_TYPE) == 0 && argc >= 2) {
        if (!s_kb) {
            reply("ERR no-keyboard-bound\n");
            return;
        }
        if (s_type[s_type_pos] != 0 || s_q_count > 0) {
            reply("ERR busy\n");
            return;
        }
        /* argv[1] onward were split on spaces; rejoin so text can contain them.
         * split() replaced each separator with a NUL, so restore them. */
        for (int i = 1; i < argc - 1; i++) argv[i][strlen(argv[i])] = ' ';
        size_t len = strlen(argv[1]);
        if (len > AGENTIO_TYPE_MAX) {
            reply("ERR too-long\n");
            return;
        }
        memcpy(s_type, argv[1], len + 1);
        s_type_pos = 0;
        reply("OK\n");
        return;
    }

    reply("ERR unknown\n");
}

void agentio_init(void)
{
    memset(s_shadow, 0, sizeof s_shadow);
    agentio_shadow_init(&s_shadow_state, s_shadow, ST7796_W, ST7796_H);
    s_line_len = 0;
    s_kb = 0;

    SEGGER_RTT_ConfigUpBuffer(AGENTIO_RTT_CHANNEL, "agentio",
                              s_up_buf, sizeof s_up_buf,
                              SEGGER_RTT_MODE_BLOCK_IF_FIFO_FULL);
    SEGGER_RTT_ConfigDownBuffer(AGENTIO_RTT_CHANNEL, "agentio",
                                s_down_buf, sizeof s_down_buf,
                                SEGGER_RTT_MODE_NO_BLOCK_SKIP);
    DIAG("agentio: up on RTT ch%u, shadow %u bytes\n",
         (unsigned)AGENTIO_RTT_CHANNEL, (unsigned)sizeof s_shadow);
}

void agentio_bind_keyboard(fw2kb_t *kb)
{
    s_kb = kb;
}

void agentio_task(void)
{
    uint8_t buf[64];
    unsigned n;

    while ((n = SEGGER_RTT_Read(AGENTIO_RTT_CHANNEL, buf, sizeof buf)) > 0) {
        for (unsigned i = 0; i < n; i++) {
            char c = (char)buf[i];
            if (c == '\r') continue;
            if (c == '\n') {
                s_line[s_line_len] = 0;
                if (s_line_len > 0) dispatch(s_line);
                s_line_len = 0;
                continue;
            }
            if (s_line_len < sizeof s_line - 1) s_line[s_line_len++] = c;
        }
    }

    /* One injected edge per call — the app's loop runs between every edge. */
    if (s_q_count > 0) {
        uartkbd_inject_set(s_queue[s_q_head]);
        s_q_head = (s_q_head + 1) % AGENTIO_QUEUE_MAX;
        s_q_count--;
    } else if (s_type[s_type_pos] != 0) {
        /* Expand the next character against the keyboard's LIVE state. */
        fw2kb_btn seq[FW2KB_CHORD_MAX];
        int n = 0;
        if (s_kb && fw2kb_chord_for(s_kb, s_type[s_type_pos], seq, &n)) {
            for (int i = 0; i < n; i++) {
                int u = fw2kb_btn_to_uartkbd(seq[i]);
                if (u >= 0 && !queue_press(u)) break;
            }
        }
        s_type_pos++;
        if (s_type[s_type_pos] == 0) {     /* string finished: reset for the next */
            s_type_pos = 0;
            s_type[0] = 0;
        }
    }

    /* Release a tap once it has been seen at least once and held long enough. */
    if (s_tap_active) {
        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (ft6336_inject_reads() > s_tap_reads0 &&
            (int32_t)(now - s_tap_deadline) >= 0) {
            ft6336_inject_set(0, 0, false);
            s_tap_active = false;
        }
    }
}

void agentio_shadow_note_window(int x0, int y0, int x1, int y1)
{
    agentio_shadow_set_window(&s_shadow_state, x0, y0, x1, y1);
}

void agentio_shadow_note_pixels(const uint8_t *bytes, size_t n)
{
    agentio_shadow_write(&s_shadow_state, bytes, n);
}

const uint8_t *agentio_shadow_fb(void)
{
    return s_shadow;
}

#endif /* FW2_AGENTIO */

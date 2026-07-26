#include "agentio/agentio.h"

#if FW2_AGENTIO

#include <string.h>
#include "agentio/agentio_proto.h"
#include "agentio/agentio_rle.h"
#include "agentio/agentio_shadow.h"
#include "display/st7796.h"
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

static void reply(const char *s)
{
    SEGGER_RTT_Write(AGENTIO_RTT_CHANNEL, s, (unsigned)strlen(s));
}

/* Dispatch one complete command line. Extended in later tasks. */
static void dispatch(char *line)
{
    if (strcmp(line, AGENTIO_CMD_PING) == 0) {
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

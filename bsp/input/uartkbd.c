#include "uartkbd.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "hardware/dma.h"
#include "pico/stdlib.h"
#include <string.h>

/* On GPIO38/39 the plain UART function is UART1 CTS/RTS; the UART-AUX
 * function routes UART1 TX/RX here instead (RP2350 datasheet GPIO muxing). */
#define UARTKBD_UART   uart1
#define UARTKBD_TX_PIN 38
#define UARTKBD_RX_PIN 39
#define UARTKBD_BAUD   62500

/* DMA drains the UART RX FIFO into this ring continuously (endless mode,
 * no IRQ), so keyboard bytes survive arbitrarily long CPU stalls up to one
 * full ring (~164 ms of line traffic at 62500 baud). uartkbd_task() reads
 * strictly behind the DMA write pointer. Alignment is required for the
 * DMA write-address ring wrap. */
#define RING_BITS 10
#define RING_SIZE (1u << RING_BITS)
static uint8_t __attribute__((aligned(RING_SIZE))) s_ring[RING_SIZE];
static uint32_t s_rd;          /* software read index into s_ring */
static int      s_dma_chan = -1;

static uartkbd_parser_t s_parser;

/* Synthesize a frame when the wire has been silent this long. The real
 * keyboard streams continuously, so this only fires when nothing is attached
 * (or it was unplugged). */
#define UARTKBD_SYNTH_IDLE_MS 50u

static uint32_t s_last_real_ms;
static uint32_t s_last_frames;

/* Idle (all-released) wire values for the button bytes. Injected buttons are
 * OR'd in by the parser, so this frame never needs per-button bit knowledge. */
static void synth_frame(void)
{
    uint8_t f[UARTKBD_FRAME_LEN];
    memset(f, 0, sizeof f);
    f[0] = 0xBD; f[1] = 0x1D;
    f[2] = 0x3F; f[3] = 0x39; f[4] = 0x80; f[5] = 0x07;
    /* Carry the last real charger bytes through so injection never clobbers
     * charger telemetry with zeros. */
    memcpy(&f[10], s_parser.charger_raw, sizeof s_parser.charger_raw);
    uint8_t sum = 0;
    for (int i = 0; i < UARTKBD_FRAME_LEN - 1; i++) sum = (uint8_t)(sum + f[i]);
    f[UARTKBD_FRAME_LEN - 1] = sum;
    for (int i = 0; i < UARTKBD_FRAME_LEN; i++) uartkbd_parse_byte(&s_parser, f[i]);
}

void uartkbd_init(void)
{
    uartkbd_parse_init(&s_parser);
    s_rd = 0;
    s_last_real_ms = to_ms_since_boot(get_absolute_time());
    s_last_frames  = 0;

    uart_init(UARTKBD_UART, UARTKBD_BAUD);
    gpio_set_function(UARTKBD_TX_PIN, GPIO_FUNC_UART_AUX);
    gpio_set_function(UARTKBD_RX_PIN, GPIO_FUNC_UART_AUX);
    uart_set_format(UARTKBD_UART, 8, 1, UART_PARITY_NONE);
    uart_set_hw_flow(UARTKBD_UART, false, false);
    uart_set_fifo_enabled(UARTKBD_UART, true);

    s_dma_chan = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config(s_dma_chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, true);
    channel_config_set_ring(&c, true, RING_BITS);      /* wrap write addr */
    channel_config_set_dreq(&c, DREQ_UART1_RX);
    dma_channel_configure(s_dma_chan, &c,
                          s_ring,                        /* write */
                          &uart_get_hw(UARTKBD_UART)->dr,/* read  */
                          0, false);                     /* count set below */
    /* Endless mode: TRANS_COUNT.MODE = 0xF -> count never decrements, the
     * channel runs forever. Writing the trigger alias starts it. */
    dma_channel_hw_addr(s_dma_chan)->al1_transfer_count_trig =
        ((uint32_t)DMA_CH0_TRANS_COUNT_MODE_VALUE_ENDLESS
             << DMA_CH0_TRANS_COUNT_MODE_LSB) | 1u;
}

void uartkbd_task(void)
{
    if (s_dma_chan < 0) return;
    uint32_t wr = (uint32_t)(dma_channel_hw_addr(s_dma_chan)->write_addr
                             - (uintptr_t)s_ring) & (RING_SIZE - 1);
    while (s_rd != wr) {
        uartkbd_parse_byte(&s_parser, s_ring[s_rd]);
        s_rd = (s_rd + 1) & (RING_SIZE - 1);
    }

    /* Synthetic idle heartbeat when the wire is silent. Emitted ONLY in hunt
     * state so it can never corrupt a partially-received real frame. */
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (s_parser.frames != s_last_frames) {
        s_last_frames = s_parser.frames;
        s_last_real_ms = now;
    }
    if (now - s_last_real_ms >= UARTKBD_SYNTH_IDLE_MS && s_parser.state == 0) {
        synth_frame();
        s_last_real_ms = now;      /* rate-limit the heartbeat to 50 ms */
        s_last_frames = s_parser.frames;
    }
}

bool     uartkbd_next_event(uartkbd_event_t *ev) { return uartkbd_parse_next_event(&s_parser, ev); }
uint16_t uartkbd_buttons(void) { return uartkbd_parse_buttons(&s_parser); }
uint8_t  uartkbd_flags(void)   { return uartkbd_parse_flags(&s_parser); }
bool     uartkbd_charger(uartkbd_charger_t *out) { return uartkbd_parse_charger(&s_parser, out); }
uint32_t uartkbd_frames(void)  { return uartkbd_parse_frames(&s_parser); }
uint32_t uartkbd_errors(void)  { return uartkbd_parse_errors(&s_parser); }

void uartkbd_inject_set(uint16_t mask)
{
    uartkbd_parse_set_inject(&s_parser, mask);
    /* Make the next task() emit a frame promptly so the edge is not delayed by
     * up to a full heartbeat interval. */
    s_last_real_ms -= UARTKBD_SYNTH_IDLE_MS;
}

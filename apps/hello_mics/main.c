// hello_mics — on-hardware smoke test for the 4-channel PDM microphone driver.
// Shows four live level meters and prints per-mic RMS + peak over RTT.
// Pass criteria: all 4 channels show low ambient at rest; tapping/speaking at
// each physical mic position (left-to-right order D, B, A, C) spikes THAT
// channel; no channel stuck at 0 / pure DC (pad-ISO or mic-power regression).
#include "fw2.h"
#include "platform/diag.h"
#include "pico/stdlib.h"
#include <stdio.h>

#define BLOCK 1600u   // 100 ms of 16 kHz PCM per pull

static int16_t s_pcm[PDM_NUM_MICS][BLOCK];

static inline uint16_t be16(uint16_t c) {
    return (uint16_t)((c >> 8) | (c << 8));
}

static void draw_level(int column, char name, unsigned rms, int peak) {
    enum { X0 = 20, Y = 80, W = 100, GAP = 15, H = 190 };
    int x = X0 + column * (W + GAP);
    int fill = peak > 2000 ? H : (peak * H) / 2000;
    if (fill < 0) fill = 0;
    if (fill > H) fill = H;
    st7796_fill_rect(x, Y, W, H - fill, be16(0x0000));
    st7796_fill_rect(x, Y + H - fill, W, fill, be16(0x07E0));
    char label[16];
    snprintf(label, sizeof label, "%c %u", name, rms);
    st7796_fill_rect(x, 282, W, 18, be16(0x0000));
    st7796_draw_text(x + 4, 286, 1, be16(0xFFFF), be16(0x0000), label);
}

static void capture_block_recoverable(void) {
    unsigned got = 0;
    while (got < BLOCK) {
        int16_t* dst[PDM_NUM_MICS];
        for (int m = 0; m < PDM_NUM_MICS; m++) dst[m] = s_pcm[m] + got;
        got += pdm_capture_pull(dst, BLOCK - got);
        fw2_app_recovery_task();
        if (got < BLOCK) tight_loop_contents();
    }
}

// Integer sqrt (no floats in DIAG, invariant 3; keep the whole path integer).
static uint32_t isqrt32(uint32_t x) {
    uint32_t r = 0, b = 1u << 30;
    while (b > x) b >>= 2;
    while (b) {
        if (x >= r + b) { x -= r + b; r = (r >> 1) + b; }
        else r >>= 1;
        b >>= 2;
    }
    return r;
}

int main(void) {
    board_init();   // 250 MHz + clk_peri re-source + I2C1 + ioexp_init
    fw2_app_recovery_init();
    DIAG("\n=== hello_mics: 4-channel PDM microphone smoke test ===\n");
    DIAG("physical mic order left->right: D, B, A, C\n");

    st7796_init();
    fw2_app_about_use_lcd();
    st7796_fill_screen(be16(0x0000));
    st7796_draw_text(12, 12, 2, be16(0xFFFF), be16(0x0000),
                     "ONBOARD PDM MICROPHONES");
    st7796_draw_text(12, 42, 1, be16(0x07FF), be16(0x0000),
                     "Physical order: D  B  A  C");
    board_backlight_set(1);

    pdm_capture_init();   // mic power on (+50 ms), pio1 SM + free-running DMA

    static const char* name = "ABCD";
    unsigned blocks = 0;
    while (1) {
        fw2_app_recovery_task();
        capture_block_recoverable();
        if (++blocks % 3 != 0) continue;   // print ~3x/s (every 3rd 100 ms block)

        unsigned rms_value[PDM_NUM_MICS];
        int peak_value[PDM_NUM_MICS];
        for (int m = 0; m < PDM_NUM_MICS; m++) {
            dcblock_inplace(s_pcm[m], BLOCK);
            uint64_t sumsq = 0;
            int peak = 0;
            for (unsigned i = 0; i < BLOCK; i++) {
                int v = s_pcm[m][i];
                if (v < 0) v = -v;
                if (v > peak) peak = v;
                sumsq += (uint64_t)((int64_t)s_pcm[m][i] * s_pcm[m][i]);
            }
            unsigned rms = (unsigned)isqrt32((uint32_t)(sumsq / BLOCK));
            rms_value[m] = rms;
            peak_value[m] = peak;
            DIAG("mic %c: rms=%5u peak=%5d   ", name[m], rms, peak);
        }
        DIAG("\n");
        static const int physical[] = { MIC_D, MIC_B, MIC_A, MIC_C };
        static const char physical_name[] = "DBAC";
        for (int column = 0; column < PDM_NUM_MICS; column++) {
            int m = physical[column];
            draw_level(column, physical_name[column], rms_value[m], peak_value[m]);
        }
    }
}

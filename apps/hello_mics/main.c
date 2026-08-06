// hello_mics — on-hardware smoke test for the 4-channel PDM microphone driver.
// RTT-only (fw rtt), no display: prints per-mic RMS + peak a few times a second.
// Pass criteria: all 4 channels show low ambient at rest; tapping/speaking at
// each physical mic position (left-to-right order D, B, A, C) spikes THAT
// channel; no channel stuck at 0 / pure DC (pad-ISO or mic-power regression).
#include "fw2.h"
#include "platform/diag.h"
#include "pico/stdlib.h"

#define BLOCK 1600u   // 100 ms of 16 kHz PCM per pull

static int16_t s_pcm[PDM_NUM_MICS][BLOCK];

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

    pdm_capture_init();   // mic power on (+50 ms), pio1 SM + free-running DMA

    static const char* name = "ABCD";
    unsigned blocks = 0;
    while (1) {
        fw2_app_recovery_task();
        capture_block_recoverable();
        if (++blocks % 3 != 0) continue;   // print ~3x/s (every 3rd 100 ms block)

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
            DIAG("mic %c: rms=%5u peak=%5d   ", name[m], rms, peak);
        }
        DIAG("\n");
    }
}

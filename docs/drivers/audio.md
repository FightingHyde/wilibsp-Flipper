# Audio (I2S full-duplex, NAU88C10)

Full-duplex I2S audio on the FreeWili2: play PCM out the NAU88C10 codec
(onboard speaker or 3.5 mm headphone/line jack) while capturing the codec ADC
(external 3.5 mm mic) — one PIO0 state machine drives both directions on a
shared I2S bus. Harvested from `evaderkrub/freewili2-fullduplex-audio` (MIT).

**Note:** this is the *I2S codec mic* (3.5 mm jack), not the on-board PDM mic
array (a separate driver — see `docs/hardware/catalog.md`).

## Pins

`PIN_AUDIO_DATA` 5 (DAC out), `PIN_AUDIO_DIN` 4 (ADC in), `PIN_AUDIO_LRCK` 6,
`PIN_AUDIO_BCLK` 7, `PIN_AUDIO_MCLK` 22 (PWM 256·fs). Codec control on I2C1
(GPIO 26/27) at address 0x1A.

## Bring-up order

```c
board_init();                 // 250 MHz; also brings up I2C1 + ioexp
codec_nau88c10_init();        // NAU88C10 register sequence (16 kHz, speaker)
codec_nau88c10_input_ok();    // optional: DIAGs rev + PM2, returns bool
codec_nau88c10_dac_mute(false);
audio_i2s_duplex_init(16000); // MCLK PWM + PIO0 SM0; RX autopush starts OFF
audio_capture_start();        // ONLY if you want mic input. Starts the ping-pong
                              // RX DMA (SHARED DMA_IRQ_0) and enables RX autopush.
                              // Playback-only apps SKIP this line entirely.
```

Note `codec_nau88c10_input_ok()` gates on the **ADC/mic** path (`reg 0x02 ==
0x0015`) as well as the silicon revision. It is a fine bring-up check for a
duplex app, but a playback-only app that uses it as a generic "is the codec
alive?" probe is over-testing — use the revision read alone if the mic path is
irrelevant to you.

## Playback

**Playback-only apps: do NOT call `audio_i2s_duplex_rx_enable()`** (and you do
not need `audio_capture_start()`). One PIO state machine clocks *both*
directions, so the RX and TX halves are not independent: if RX autopush is on
and nothing drains the RX FIFO, the `in pins,1` stalls the state machine the
moment that 4-deep FIFO fills — about 4 frames, ~250 µs at 16 kHz. A stalled SM
stops driving BCLK/LRCK, so **the DAC dies with it**.

**The symptom to recognise: you hear a CLICK per note instead of a tone**, with
every codec register reading correct. Each note gets only its first ~250 µs — a
fraction of one cycle — before the SM wedges. It is not permanent silence,
because any code path that calls `audio_i2s_duplex_play_stop()` between notes
clears the FIFOs and briefly unwedges the SM, so a chime degrades into a burst
of clicks with no pitch. Measured on a mic (RP2350 rev 3, 2026-07-25): tonal
magnitude at the intended frequency fell from ~1100 to <4 — about 300x — and
the dominant frequency stopped tracking the note at all.

`audio_i2s_duplex_init()` therefore leaves autopush **off**, and
`audio_capture_start()` turns it on for you. Confirm a suspected stall by
reading `PIO0->FDEBUG` (`0x50200008`): `RXSTALL` is bits 3:0 and is sticky.

The TX path is a zero-CPU DMA read-ring over a pre-filled buffer. The buffer
must hold whole tone periods, be a power-of-two **bytes**, and be aligned to
its byte size:

```c
static uint32_t tone[64] __attribute__((aligned(256))); // 64 frames = 256 bytes
// fill tone[i] = ((uint32_t)left16 << 16) | right16;   // e.g. via tone_gen_fill
audio_i2s_duplex_play_loop(tone, 64);
audio_i2s_duplex_play_stop();                 // park DAC at silence
codec_nau88c10_set_output(CODEC_OUT_SPEAKER); // or CODEC_OUT_HEADPHONE (3.5mm)
```

For buffers too large (or not power-of-two / aligned) for the read-ring —
e.g. sampled audio clips — use the **streamed** loop instead:

```c
audio_i2s_duplex_play_stream_loop(clip, frames); // frames: multiple of 8192
```

Two chained DMA channels refill in 8192-frame chunks from a shared
`DMA_IRQ_0` handler and wrap seamlessly; `frames` **must be a multiple of
8192** (a refill always transfers a whole chunk). No alignment requirement.
`audio_i2s_duplex_play_stop()` stops either loop.

When playback is done and the speaker should not idle-hiss, power the output
stage down:

```c
codec_nau88c10_speaker_low_power(); // soft-mute DAC, mute spk, HP off, 5V boost off
```

To bring the output back: `codec_nau88c10_dac_mute(false)` (clears the DAC
soft-mute) then `codec_nau88c10_set_output(...)` (re-enables the output
drivers, volume, and boost).

## Capture ("audio in")

```c
if (audio_capture_block_ready()) {
    const uint32_t *b = audio_capture_block();      // 256 frames [L16|R16], or NULL
    if (b) {
        uint16_t pk = vu_peak(b, AUDIO_CAPTURE_BLOCK_FRAMES, AUDIO_MIC_I2S_SLOT);
        // ... your DSP over the raw PCM block ...
    }
}
```

The mono NAU88C10 ADC streams on the **right** slot (`AUDIO_MIC_I2S_SLOT = 1`).

## Constraints & gotchas

- **Sample rate** is fixed at 16 kHz in the current codec register set. The
  driver derives MCLK + PIO clkdiv from `clk_sys`, so it runs at the board's
  250 MHz unchanged.
- **LRCK is locked to MCLK/256, not to the nominal fs** (see `facts.md`). MCLK is
  an integer PWM divide of `clk_sys` (250e6/61 = 4.0984 MHz, not 4.096), so the
  MCLK-direct codec runs fs = 16009 Hz; the PIO frame divider is derived from the
  same integer so data-in == data-out. Setting LRCK to the nominal 16000 instead
  reintroduces a ~9 Hz DAC-slip tick. Net pitch is +0.06 %, inaudible.
- **DMA_IRQ_0 is shared** with the ST7796 flush — `audio_capture` uses
  `irq_add_shared_handler`; never register an exclusive handler on it.
- **RX slot/phase alignment** is the known bring-up risk: if captured peaks sit
  at the noise floor while the mic is driven, apply the knobs documented in
  `bsp/audio/i2s_duplex.pio` (flip `AUDIO_MIC_I2S_SLOT`, move the `in pins,1` to
  the other BCLK phase, or swap the channel side values).
- No floats over RTT: `DIAG` integer values only.

See `apps/hello_audio/main.c` for a complete worked example.

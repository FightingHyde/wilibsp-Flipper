# agentio — remote input injection + screen capture for agent E2E verification

**Date:** 2026-07-25
**Status:** design approved, not implemented

## Problem

An agent working on this BSP can build, flash, and read RTT diagnostics, but it
cannot press a button, touch the panel, or see what the panel shows. Every
end-to-end claim therefore depends on a human sitting at the hardware. This
design closes that loop: the agent drives input and captures the screen as a
PNG it can look at.

## Hardware constraints that shape the design

1. **The LCD cannot be read back.** SPI1's RX pin is GPIO8, which is wired as
   the LCD's DC output (`board.h`: `PIN_LCD_DC 8`, also `PIN_CC1101_MISO`), and
   the panel's SDO is not connected. `RAMRD` (0x2E) readback is impossible.
   Capture must come from a shadow framebuffer the BSP maintains as it draws.
2. **The host↔target link already exists in both directions.** SEGGER RTT has
   down-channels (host→target), and OpenOCD's `rtt server start <port> <chan>`
   serves a channel bidirectionally. `fw rtt` already proves this path on
   hardware. No new wire is needed.
3. **Only one process can own the debug probe.** Any tooling must share a
   single OpenOCD instance between diagnostics and the harness.
4. **Apps are `copy_to_ram`** (512 KB SRAM budget), so a 300 KB shadow
   framebuffer lives in PSRAM, not SRAM.
5. **PSRAM offset 0 is already occupied.** `hello_keyboard` and
   `hello_charger` both place a 480x320 framebuffer at raw `PSRAM_BASE`.
   `bsp/platform/psram_layout.h` — which claims to be the single source of
   truth for PSRAM regions — is a stale harvest artifact: it `#include`s
   `ui/screen_analyzer.h`, which does not exist in this repo, so nothing can
   include it. The shadow buffer must claim a documented offset well clear of
   zero.

## Scope

**In:** button injection (all 14 `uartkbd_btn_t`), touch injection,
high-level `type "text"` via the fw2kb chord engine, and PNG capture of the
ST7796 panel and of the DVI/HSTX framebuffer (with a crop rect, which is how
the DVI OSD region is served).

**Out:** touch gestures/swipes with interpolated points; faked charger
telemetry and AUDIO/HOTPLUG/USB detect flags; ASCII-art or hash-based capture
formats; a scripted E2E test runner. Each is a straightforward later addition
on top of the primitives below.

## Architecture

New module `bsp/agentio/`, guarded by CMake option `FW2_AGENTIO` (default ON).
When OFF, every hook compiles to nothing: no PSRAM shadow, no RTT buffers, no
cycles in the draw path.

| File | Responsibility |
| --- | --- |
| `agentio.{h,c}` | RTT channel setup, command decode, dispatch, capture streaming, injection queue |
| `agentio_shadow.{c,h}` | **Pure** window-walking: window rect + wire-order byte stream → buffer writes. No hardware includes. |
| `agentio_rle.{c,h}` | **Pure** PackBits-16 encoder |
| `agentio_proto.h` | Opcodes and header layout, mirrored by the Python host |

App-facing surface is three calls:

```c
agentio_init();                 /* after psram_init() and st7796_init() */
agentio_bind_keyboard(&kb);     /* optional; required only for `type` */
for (;;) {
    /* ... app work ... */
    agentio_task();             /* drain commands, service captures */
}
```

`agentio_task()` is a no-op when `FW2_AGENTIO` is OFF, so the call can stay in
an app unconditionally.

### Transport

RTT in both directions. `agentio_init()` configures a dedicated up-buffer
(pixels, ~4 KB) and down-buffer (commands, 256 B) via
`SEGGER_RTT_ConfigUpBuffer` / `ConfigDownBuffer`. Channel 0 stays exclusively
`DIAG()`.

Rejected alternatives:

- **OpenOCD memory access** (`mww` mailbox + `dump_image` of the shadow):
  least target code, but bets the design on OpenOCD reading RP2350 XIP-CS1
  (PSRAM at `0x11000000`) over SWD, which is unverified here and would fail
  late. It also still needs a target-side mailbox poll for injection, so it
  does not actually remove the module.
- **Hybrid** (RTT commands, `dump_image` pixels): carries the same PSRAM read
  risk *and* builds two transports.

## Capture

### Shadow framebuffer

`st7796_set_window()` records the current rect in a static. Every
`spi_write_blocking` in the pixel-push paths also calls
`agentio_shadow_write()`:

- `st7796_fill_screen()` — per row
- `st7796_fill_rect()` — per row
- `st7796_blit_rect()` / `st7796_draw_text()` — via `push_pixels()`
- `st7796_flush_async()` — at DMA-trigger time (the source buffer is valid
  until the done-callback, so mirroring at trigger is safe)

One tap point therefore covers direct drawing (`hello_display`), app-owned
PSRAM framebuffers flushed whole-screen (`hello_keyboard`, `hello_charger`),
and LVGL (`orca_browser`, whose display port flushes through
`st7796_flush_async`).

The shadow is 480x320 **wire-order** (big-endian) RGB565 = 307,200 bytes, at
`PSRAM_BASE + 0x00600000` (6 MB in). That clears both the app framebuffers at
offset 0 and the capture-clip convention in `psram_layout.h`, which spans 1 MB
to 5 MB (`PSRAM_CAPTURE_OFFSET 0x100000` plus `CAPTURE_MAX_DURS` = 4 MB of
durations), and leaves the buffer inside the 8 MB device. The offset is defined
in `agentio.h`; `psram_layout.h` is left alone (fixing its dead include is out
of scope for this work).

### DVI surface

No shadow needed. `hstx_dvi_video_base()` is a live SRAM framebuffer, read
directly. It differs from the LCD surface in two ways the encoder handles
per-surface: pixels are **native-endian** (no byte swap) and rows are strided
(`hstx_dvi_video_stride()` uint16 elements, because pixels are interleaved
with HSTX command words).

### Wire format

Command: capture with a surface (`lcd` | `dvi`), an optional crop rect, and an
optional integer downscale (nearest-neighbour, pixel skipping).

Response: a fixed header (magic, surface, format, x, y, w, h as u16, payload
length as u32) followed by a PackBits-16 payload. PackBits (literal runs +
repeat runs) rather than plain RLE: plain RLE doubles the size of photo-like
content, PackBits bounds the worst case at about +1%.

Downscale exists because throughput is the real constraint. A flat UI screen
compresses to a few KB and arrives in well under a second; a photo-like screen
such as `orca_browser` stays near 300 KB, and RTT over CMSIS-DAP realistically
runs at tens of KB/s. `--scale 2` is a few lines on the target and the
difference between a ~2 s and a ~30 s capture.

### Coherence

Capture runs inside `agentio_task()` — a quiescent point in the app's loop —
and **blocks** the loop until fully streamed. This yields a tear-free snapshot.

Accepted cost: a long capture stalls the app. The keyboard's DMA ring only
covers ~164 ms of line traffic, so button frames can be dropped during a
capture. For screenshotting a settled screen, which is the actual use case,
this is the right trade. Chunked non-blocking streaming is the alternative if
this ever bites; it is deliberately not built now.

## Input injection

### Buttons

`uartkbd_parse` gains an `inject_mask` field plus
`uartkbd_parse_set_inject(p, mask)`. `decode_buttons()` ORs the mask in
**after** the active-low inversion and **before** edge detection.

That placement is load-bearing: the real keyboard streams frames continuously,
so an injected hold applied anywhere above the parser would be cancelled by
the very next hardware frame reporting the button idle. ORing inside decode
makes a hold survive real traffic, and keeps the logic pure and host-testable.

Injection must also work with **no keyboard attached**, where no frames arrive
and no edges are ever produced. So `uartkbd.c` synthesizes a well-formed
23-byte frame whenever the inject mask is non-zero or has just changed and no
checksum-valid real frame has arrived in the last 50 ms: correct sync (0xBD 0x1D),
active-low button bits, additive 8-bit checksum of bytes 0-21, and the
last-known charger bytes (`s_parser.charger_raw`) copied through so injection
never clobbers charger telemetry with zeros. It is emitted **only while the
parser is in hunt state** (`state == 0`), so it can never corrupt a
partially-received real frame.

Injected input therefore exercises the genuine parse path — sync hunt,
checksum, priming, edge detection — rather than bypassing it.

Public binding API (`uartkbd.h`):

```c
void uartkbd_inject_set(uint16_t mask);   /* bit N = uartkbd_btn_t N held */
```

### Touch

Injection goes in `ft6336_poll()`, the single touch entry point for every app
including LVGL's input device read. A static injected point short-circuits the
I2C read while active, and a counter records how many polls observed it:

```c
void     ft6336_inject_set(uint16_t x, uint16_t y, bool down);
uint32_t ft6336_inject_reads(void);
```

Tap policy lives in `agentio`: release the point once it has been observed at
least once **and** the requested duration has elapsed. Without the
observed-at-least-once condition, an app polling slower than the tap duration
would miss the tap entirely.

### `type "text"`

A new pure function on the chord engine:

```c
bool fw2kb_chord_for(const fw2kb_t *kb, char ch, fw2kb_btn *out, int *n);
```

It walks the existing `k_pages[5][5]` tables to produce the press sequence for
`ch` given the keyboard's current state: a PAGE press first if mid-chord (to
cancel) or if the character lives on another page, then the group button, then
the character button.

Resolution happens **on the target**, not in Python, for two reasons: it
respects the app's current page and mode, and it avoids duplicating the chord
tables into the host where they would drift from the C source.

`type` requires `agentio_bind_keyboard(&kb)`. Unbound, it returns an error
rather than guessing which keyboard instance the app owns.

### Queue timing

The press queue drains **one edge per `agentio_task()` call**: press (mask set)
on one iteration, release (mask clear) on the next. Since the harness and the
app share a single thread, this guarantees the app's loop runs between every
edge — no millisecond delays, no guessing at the app's poll rate, fully
deterministic. Worst case is about six loop iterations per character.

## Host tooling (`tools/fw.py`)

```
fw screenshot [-o out.png] [--surface lcd|dvi] [--crop x,y,w,h] [--scale N]
fw press <btn>[,<btn>...]
fw hold <btn>
fw release <btn>
fw touch <x> <y> [--down|--up]
fw type "text"
```

Stdlib only, matching the existing CLI. PNG output is ~30 lines of `zlib` +
`struct` (no Pillow dependency).

**Probe sharing.** `fw rtt` starts serving **both** channels — 0 (diag) on
port 9090 and the agentio channel on 9091 — so a running `fw rtt` doubles as
the probe-holding session. One-shot commands reuse port 9091 when it is
listening, and otherwise spawn their own OpenOCD and tear it down afterwards.
This makes "stream diagnostics while driving input" the default rather than a
conflict. No separate `fw session` verb is added.

## Testing

**Host CTest (`tests/`, no hardware):**

- `test_agentio_shadow.c` — window walk: clipping, multi-row windows, writes
  split across calls, windows at panel edges
- `test_agentio_rle.c` — PackBits round-trip, including the incompressible
  worst case (bounded expansion) and the all-one-colour best case
- `test_uartkbd_inject.c` — inject mask ORs into edge detection; a synthesized
  frame passes the real checksum and parser; synthesis never corrupts a
  partially-received frame
- `test_fw2kb_chord.c` — property test: for every printable character on every
  page, resolve the chord and replay the presses through `fw2kb`, asserting it
  produces that character

**Python:** round-trip test for the PackBits decoder and the PNG writer, in
`tools/tests/`.

**On hardware:**

- New `apps/hello_agentio` draws a known pattern (colour bars plus text) so
  `fw screenshot` output can be validated against a known-good image.
- `agentio` enabled in `hello_keyboard` for the full loop: `fw type "hello"`
  followed by `fw screenshot` should show HELLO on the panel.

## Documentation

- `docs/drivers/agentio.md` — usage, wire protocol, PSRAM offset, limitations
- `AGENTS.md` — pointer in "Where things live", and the new `fw` verbs in the
  command vocabulary table
- `docs/hardware/catalog.md` — row for the harness

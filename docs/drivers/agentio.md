# Agent E2E harness (`bsp/agentio/`) — remote input injection + screen capture

**Not a peripheral driver** — a test harness that lets a host (an AI agent or a
human) drive the board over the existing debug probe: press buttons, touch the
panel, type text through the fw2kb chord engine, and pull the screen back as a
PNG. It exists so on-hardware behavior can be checked automatically instead of
by eyeballing the panel.

**Status: on-hardware verification is still PENDING.** Nothing in this
harness — the shadow framebuffer, the RTT command channel, the PackBits-16
encoder, or the `fw` CLI verbs — has been confirmed running against a real
board yet. Treat everything below as design intent until a findings doc
records a hardware run.

## What it does

- **Input injection** — `fw press` / `hold` / `release` inject FW2 button
  edges into `uartkbd`'s decode path; `fw touch` injects a point into
  `ft6336_poll`'s path; `fw type` drives the fw2kb chord engine through
  `agentio_bind_keyboard`. From the app's point of view an injected press is
  indistinguishable from a real one — it comes out of the same
  `uartkbd_next_event()` / `ft6336_poll()` / `fw2kb_next_event()` calls the app
  already reads.
- **Screen capture** — the ST7796 panel is write-only from this MCU (there is
  no readback path), so `bsp/agentio/agentio_shadow.c` mirrors every
  `CASET`/`RASET`/pixel-write the display driver issues into a PSRAM shadow
  framebuffer. `fw screenshot` asks the target to walk that shadow (or, for
  `--surface dvi`, the live DVI video framebuffer) row by row, PackBits-16
  encode it, and stream it back over RTT; the host decodes it to a PNG.

## The three app calls

Call these from `main()`, after `board_init()`/`st7796_init()` and (if you
want `TYPE` to work) after `fw2kb_init()`:

```c
agentio_init();                 // after board_init() and st7796_init()
agentio_bind_keyboard(&kb);     // optional; required only for TYPE
for (;;) { ...app work...; agentio_task(); }
```

**`agentio_init()` must run before anything you want to appear in a capture
is drawn.** It `memset`s the shadow framebuffer to zero, and the shadow only
records writes issued after that call — anything drawn earlier is erased
from the shadow (though it still reaches the physical panel), so a capture
of it comes back black. Call `agentio_init()` immediately after
`board_init()`/`st7796_init()` (and `uartkbd_init()`/`fw2kb_init()` if you
want `TYPE` and button injection), before the first draw call.

`apps/hello_agentio` is the worked example — a known-pattern app whose output
a capture can be checked against by eye or by an agent:

```c
int main(void)
{
    board_init();
    size_t psram_bytes = psram_init();
    if (psram_bytes < (size_t)ST7796_W * ST7796_H * 2) {
        DIAG("hello_agentio: PSRAM absent/too small (%u bytes) - halting\n",
             (unsigned)psram_bytes);
        for (;;) tight_loop_contents();
    }
    st7796_init();
    ft6336_init();
    board_backlight_set(1);

    // agentio_init() must come before the drawing below — it zeroes the
    // shadow framebuffer, so anything drawn first would be erased from it.
    fw2kb_t kb;
    uartkbd_init();
    fw2kb_init(&kb);
    agentio_init();
    agentio_bind_keyboard(&kb);

    const int n = (int)(sizeof k_bars / sizeof k_bars[0]);
    const int bar_w = ST7796_W / n;
    for (int i = 0; i < n; i++) {
        st7796_fill_rect(i * bar_w, 0, bar_w, 200, k_bars[i]);
        st7796_draw_text(i * bar_w + 4, 208, 1, BE(0xFFFF), BE(0x0000),
                         k_labels[i]);
    }
    st7796_draw_text(8, 240, 2, BE(0xFFFF), BE(0x0000), "hello_agentio");
    st7796_draw_text(8, 268, 1, BE(0x07E0), BE(0x0000),
                     "fw screenshot -o shot.png");
    DIAG("hello_agentio: pattern drawn, agentio up\n");

    for (;;) {
        uartkbd_task();

        uartkbd_event_t bev;
        while (uartkbd_next_event(&bev)) {
            if (!bev.pressed) continue;
            DIAG("hello_agentio: btn %u\n", (unsigned)bev.btn);
            // Paint a swatch so an injected press shows up in a capture.
            st7796_fill_rect(8 + 20 * (int)bev.btn, 296, 16, 16, BE(0x07E0));
        }

        uint16_t tx, ty;
        if (ft6336_poll(&tx, &ty))
            st7796_fill_rect((int)tx - 4, (int)ty - 4, 8, 8, BE(0xF81F));

        agentio_task();
    }
}
```

It draws eight labelled 60-px colour bars (RED/GRN/BLU/YEL/CYN/MAG/WHT/BLK),
two text lines, paints a green swatch in the button row on each injected
press, and a magenta dot at each touch — a capture can be diffed against
exactly what the code above says it drew.

`apps/hello_keyboard` wires the same three calls into an existing app: after
its `fw2kb_init(&s_kb)` it adds `agentio_init(); agentio_bind_keyboard(&s_kb);`,
and its main loop's last statement is `agentio_task();`.

## Build switch: `FW2_AGENTIO`

`option(FW2_AGENTIO "Build the agentio remote-control/capture harness" ON)` in
the top-level `CMakeLists.txt`. **OFF** collapses every entry point in
`bsp/agentio/agentio.h` to an empty `static inline`, so the three calls above
can stay in an app unconditionally at zero cost:

- No PSRAM shadow allocation (the 307,200-byte `s_shadow` buffer disappears).
- No RTT channel 1 buffers (`s_up_buf` 4 KB, `s_down_buf` 256 B) and no
  capture row scratch (`s_row`/`s_enc`, ~2 KB) in `.bss`.
- No injection hooks compiled into the *draw* path — `agentio_shadow_note_window`/
  `_note_pixels` collapse to nothing, so `st7796.c` carries zero overhead.

**This is not a whole-harness guarantee — only `bsp/agentio/agentio.c` and
its draw-path hooks collapse.** `FW2_AGENTIO=0` does *not* remove the
injection plumbing on the input side, because that code was written directly
into the input drivers, not gated behind the same switch:
`ft6336.c` still carries `s_inj_down`/`s_inj_x`/`s_inj_y` and always checks
them in `ft6336_poll()`; `uartkbd_parse.c`'s button decode always
`| p->inject_mask`s in the injected bits; and `uartkbd_init()` always primes
the parser with one synthetic frame at boot regardless of the switch. The
bytes are a handful and the behavior is inert with no `fw` client attached,
but "zero cost" above describes the capture/RTT/draw-hook side only.

Because `s_shadow` (and the rest of `agentio.c`) is referenced only from
`agentio_init()`, `--gc-sections` strips the whole harness out of any app
that never calls it, even with `FW2_AGENTIO=ON` — `hello_agentio` and
`hello_keyboard` are the first two apps that actually pull it in, which is
why their `size -A` output is the place these numbers first become visible
(see "Verified sizes" below).

## `fw` verbs

All examples assume the debug probe is attached (`fw rtt` or any agentio verb
will spawn an OpenOCD RTT session itself if one is not already running, and
reuse it if one is).

| Command | What it does |
|---|---|
| `fw screenshot -o shot.png [--surface lcd\|dvi] [--crop x,y,w,h] [--scale N]` | Capture the screen to a PNG. `--surface` defaults to `lcd` (the ST7796 shadow); `dvi` reads the live DVI video framebuffer. `--crop` selects a sub-rectangle (default: the whole surface); `--scale N` nearest-neighbour downsamples by N. |
| `fw press <btn>[,<btn>...]` | Inject a tap (press+release, ~60 ms apart) on one or more buttons, e.g. `fw press green` or `fw press grey,red`. |
| `fw hold <btn>[,<btn>...]` | Inject a sustained press (sets the button mask and leaves it set) — pairs with `fw release`. |
| `fw release <btn>[,<btn>...]` | Clear the injected button mask (the argument is accepted but ignored — release always clears everything). |
| `fw touch <x> <y> [--down\|--up]` | Inject a touch. With neither flag: a ~60 ms tap. `--down` sets the point and leaves it held; `--up` releases (x/y are ignored on release). |
| `fw type "text"` | Type text through the fw2kb chord engine — each character is expanded to its chord (1-2 button presses) against the keyboard's live page state. Requires a keyboard bound with `agentio_bind_keyboard`. |

Button names (`BUTTONS` in `tools/fw.py`): `grey`, `yellow`, `green`, `blue`,
`red`, plus the nav buttons — see `tools/fw.py` for the authoritative list and
current index mapping to `uartkbd`'s bit positions.

**`fw type "hello"` then `fw screenshot` is not automatically safe.** `TAP`
and `TYPE` reply `OK` as soon as the edge/string is *accepted* into the
target's drain queue — not once it has been applied. `dispatch()` runs a
`CAP` synchronously, but the queue only drains one edge per
`agentio_task()` call, which happens *after* `dispatch()` returns. A
`screenshot` issued immediately after a `type`/`press`/`touch` reply can
therefore land before, or partway through, that injection. `CAP` detects
this and replies `ERR busy` while a press or `TYPE` string is still
draining; on that reply, wait and retry the capture rather than treating it
as a hard failure.

Examples:

```bash
python tools/fw.py flash hello_agentio
python tools/fw.py screenshot -o shot.png
python tools/fw.py press green
python tools/fw.py touch 240 160
python tools/fw.py screenshot --crop 0,280,480,40 -o buttons.png
python tools/fw.py flash hello_keyboard
python tools/fw.py type "hello"
```

## Wire protocol

Transport: SEGGER RTT channel 1 (`AGENTIO_RTT_CHANNEL`), separate from
channel 0 (`DIAG()`only — never shared). Commands are ASCII, newline-terminated,
sent on the DOWN buffer; replies go out on the UP buffer.

| Command | Args | Reply | Notes |
|---|---|---|---|
| `PING` | — | `OK` | Liveness check. |
| `BTN <mask hex>` | 16-bit hex button mask | `OK` | Sets the injected button mask directly (used by `hold`/`release`). |
| `TAP <btn>` | button index (decimal) | `OK` / `ERR queue` | Queues a press+release edge pair, consumed one edge per `agentio_task()` call so the app cannot miss it. |
| `TCH <x> <y> <mode>` | decimal x, y; mode 0=up, 1=down, 2=tap | `OK` / `ERR mode` | Tap (`mode=2`) auto-releases after the app has observed it at least once **and** `AGENTIO_TAP_MS` (60 ms) has elapsed. |
| `TYPE <text>` | rest of the line verbatim (spaces allowed) | `OK` / `ERR no-keyboard-bound` / `ERR busy` / `ERR too-long` | Up to `AGENTIO_TYPE_MAX` (64) characters; rejected while a previous `TYPE` or button queue is still draining. **`OK` means the string was accepted into the drain queue, not that it has been typed** — see the capture/injection race note below. |
| `CAP <surface> <x> <y> <w> <h> <scale>` | all decimal; surface 0=LCD, 1=DVI | 18-byte header + PackBits-16 payload, or an `ERR` line | See capture header layout below. `w`/`h` of 0 (or out of range) clamp to the remaining surface extent from `(x,y)`. Replies `ERR busy` while a `TAP`/`TYPE` injection is still draining — see below. |

Any unrecognized line replies `ERR unknown`.

### Capture header layout (`AGENTIO_HEADER_LEN` = 18 bytes, all multi-byte fields big-endian)

| Offset | Size | Field | Meaning |
|---|---|---|---|
| 0 | 4 | magic | `"FW2C"` |
| 4 | 1 | surface | `AGENTIO_SURFACE_LCD` (0) or `AGENTIO_SURFACE_DVI` (1) |
| 5 | 1 | format | `AGENTIO_FORMAT_PACKBITS16` (0) — only format defined today |
| 6 | 2 | x | crop origin actually used |
| 8 | 2 | y | crop origin actually used |
| 10 | 2 | w | output width, after scaling |
| 12 | 2 | h | output height, after scaling |
| 14 | 4 | payload_len | bytes of PackBits-16 payload that follow |

Payload: PackBits-16 (`bsp/agentio/agentio_rle.{c,h}`), rows encoded
independently and concatenated. Control byte, read as a signed int8:

- `0..127` → `n+1` literal units follow.
- `-127..-1` → the next unit repeats `1-n` times.
- `-128` → never emitted.

A "unit" is one RGB565 colour value serialized big-endian (2 bytes). A
validation failure (bad surface, empty rect after clamping, zero output
size) replies with a plain `ERR <reason>` line instead of the 18-byte header —
the host tells these apart by checking the first 4 bytes against
`AGENTIO_MAGIC` before trying to parse a header.

## Limitations

- **Capture blocks the app loop — with interrupts masked, not just the loop
  stalled.** `do_capture()` walks every output row synchronously inside
  `agentio_task()`, and the RTT up-buffer is configured
  `SEGGER_RTT_MODE_BLOCK_IF_FIFO_FULL` — a capture must not silently drop
  pixels, so it blocks until the host has drained the whole payload.
  SEGGER's blocking write spins with `BASEPRI` raised, i.e. **with
  interrupts masked for the duration**, not merely the app's own loop
  paused. Any IRQ-driven subsystem glitches during a capture — audio in
  particular. The keyboard DMA ring and the DVI scanout are unaffected
  by this specific effect because both are IRQ-free by design (the UART DMA
  ring drains in software from `uartkbd_task()`, and the HSTX scanout is a
  zero-IRQ chained DMA) — but a keyboard frame can still be lost if the
  capture's *total* stall exceeds the ring's ~164 ms budget (see
  `docs/drivers/keyboard.md`), independent of the interrupt-masking effect.
- **If the host disappears mid-capture, the target hangs until a power
  cycle.** A capture spins with interrupts masked until the RTT up-buffer
  drains; if the host goes away first — Ctrl+C, or the CLI's cleanup path
  killing OpenOCD after an exception — nothing ever drains it, so the wait
  never ends. There is no watchdog in this BSP to recover from that. Treat a
  large capture as something you do not want to interrupt.
- **DVI capture requires DVI to already be running.** `--surface dvi` reads
  `hstx_dvi_region_base()`/`hstx_dvi_video_stride()`/`hstx_dvi_region_h()`
  (plus the fixed `HSTX_VID_W_MAX` for width); until `hstx_dvi_init()` has
  been called those are zero/unset and `CAP` replies `ERR surface`. It
  deliberately reads the *region* accessors, not `hstx_dvi_video_base()`/
  `_w()`/`_h()` (the movie sub-rect) — per `bsp/display/hstx_dvi.h` the OSD
  draws in the margin outside the movie rect, so cropping the movie alone
  could never reach it. **No app in the tree initializes both DVI and
  agentio yet, so this path is unexercised** — treat it as design intent
  pending a real test.
- **`TYPE` needs a bound keyboard and has a length cap.** Without
  `agentio_bind_keyboard()` having been called, `TYPE` replies
  `ERR no-keyboard-bound`. Text longer than `AGENTIO_TYPE_MAX` (64
  characters) replies `ERR too-long` — split longer input across multiple
  `fw type` calls.
- **The shadow reflects what the driver was told to draw, not what the panel
  physically shows.** `agentio_shadow_note_window`/`_note_pixels` mirror the
  byte stream `st7796.c` sends toward the panel; they cannot detect a wiring
  fault, a panel that ignored a command, or any other physical-layer failure.
  A capture proves the app issued the expected drawing calls — it is not
  independent proof the LCD lit up correctly.
- **One capture/injection session at a time.** There is no queuing across
  `fw` invocations beyond what's described above (button/TYPE queues); two
  concurrent `fw` processes will contend for the same debug probe and RTT
  connection.
- **Injection state is a single global mask, not per-caller.** `fw hold
  green` sets the mask directly (`BTN`); a later `fw press`/`fw type` in a
  separate invocation queues edges against — and can clobber — that same
  mask. There is no "hold this, inject that on top, then restore the hold"
  composition. Relatedly, `fw press a,b` opens a separate OpenOCD/RTT
  session per button rather than one session for the whole list, so a
  multi-button press is several independent round trips, not one atomic
  command.

## Dependencies

`bsp/agentio/agentio.c` depends on `display/st7796.h` (shadow mirroring
hook), `display/hstx_dvi.h` (DVI surface), `input/ft6336.h` /
`input/uartkbd.h` (injection targets), and `keyboard/fw2kb.h` (TYPE
expansion). `agentio_rle.{c,h}` and `agentio_shadow.{c,h}` are pure and
host-tested (`tests/test_agentio_rle.c`, `tests/test_agentio_shadow.c`) with
no hardware includes.

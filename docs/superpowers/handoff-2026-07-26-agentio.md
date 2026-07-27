# Handoff — agentio (2026-07-26)

> **UPDATE, same day: hardware verification is DONE and PASSED.** See
> `docs/superpowers/findings/2026-07-26-agentio-e2e.md`. The "run it on
> hardware" section below is kept as the reproduction recipe.

**Branch:** `agentio` (git worktree at `.claude/worktrees/agentio`), on top of
`master` @ 6fd4a5e. Hardware-verified and ready to merge.

**What it is:** remote input injection + screen capture, so an agent can drive
the board and see the panel without a human present. Design:
`docs/superpowers/specs/2026-07-25-agentio-design.md`. Plan:
`docs/superpowers/plans/2026-07-26-agentio.md`. Usage:
`docs/drivers/agentio.md`.

## State

Verified without hardware: 34/34 C tests, 18/18 Python tests, all 12 apps build
with `FW2_AGENTIO` both ON and OFF. `.psram_noload` is 307200 for
`hello_agentio` (the shadow) and 614400 for `hello_keyboard` (its own
framebuffer plus the shadow); `.bss` grew ~6.9 KB, far under the 512 KB budget.

Per-task reviews plus a whole-branch review are clean. The whole-branch review's
8 findings were fixed in commits `60d0c6b..7dac72a` and re-reviewed.

## The hardware run (done — this is the reproduction recipe)

Needs a CMSIS-DAP probe (the FreeWili 2 exposes several debug interfaces — use
**OpenOCD interface 0**, per AGENTS.md). Verified 2026-07-26 with a Raspberry Pi
Debug Probe (`2e8a:000c`).

```bash
python tools/fw.py build hello_agentio
python tools/fw.py flash hello_agentio
python tools/fw.py screenshot -o shot.png
```

`shot.png` should be 480x320 showing eight labelled colour bars (RED GRN BLU YEL
CYN MAG WHT BLK) across the top 200 rows, "hello_agentio" at y=240, and
"fw screenshot -o shot.png" at y=268. Compare against `apps/hello_agentio/main.c`
— it is written so the capture can be diffed against exactly what the code drew.

```bash
python tools/fw.py press green
python tools/fw.py touch 240 160
python tools/fw.py screenshot -o shot2.png
```

`shot2.png` should additionally show a green swatch in the button row (y=296) and
a magenta dot at (240,160).

```bash
python tools/fw.py flash hello_keyboard
python tools/fw.py type "hello"
python tools/fw.py screenshot -o kb.png
```

`kb.png` should show `hello` in the text area. Note `fw type` returns `OK` when
the string is *accepted*, not when it has finished typing — `CAP` answers
`ERR busy` while injection is still draining, so retry the screenshot.

Results are recorded in `docs/superpowers/findings/2026-07-26-agentio-e2e.md`.
All of the above passed on the first attempt.

## Two risks that remain unprovoked

Neither was triggered during the hardware run. Both are documented in
`docs/drivers/agentio.md` and are consequences of the approved blocking-capture
design, but they are sharper than the spec anticipated:

1. **A capture blocks with interrupts masked**, not merely with the app loop
   stalled — SEGGER's `BLOCK_IF_FIFO_FULL` write spins inside a lock that sets
   `BASEPRI`. IRQ-driven audio will glitch during a capture. The keyboard DMA
   ring and the DVI scanout are unaffected (both are IRQ-free by design).
2. **If the host disappears mid-capture** (Ctrl+C on a slow screenshot, or the
   CLI killing OpenOCD on an exception), the target spins forever with
   interrupts masked. There is no watchdog in this BSP — the board needs a power
   cycle. Easy to trip on a full-resolution capture of a photo-like screen; use
   `--scale 2` to keep captures short.

If either bites, the fix is to replace the blocking RTT write with
`SEGGER_RTT_GetAvailWriteSpace()` plus a non-blocking write in a spin loop with
a timeout that aborts the capture. That was scoped out of this branch.

## Also unexercised

No app in the tree initializes both DVI and agentio, so the DVI capture surface
remains unexercised even after the hardware run. If you want it, wire
`agentio_init()` into `apps/hello_dvi` first.

## Deferred minors

`.superpowers/sdd/2026-07-26-agentio/progress.md` (git-ignored) holds the full
execution ledger, including ~10 deferred minor findings the whole-branch review
triaged as safe to defer.

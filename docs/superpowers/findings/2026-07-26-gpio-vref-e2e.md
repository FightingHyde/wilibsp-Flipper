# GPIO reference voltage (VIO) select end-to-end on hardware — 2026-07-26

**Result: PASS**, with one open question (see "Not verified / open" below).
`ioexp_vref()` was added to the PCAL6524 driver and all five selections were
exercised on a physical FreeWili 2, measured on-board via the display CPU's own
rail monitors. `VREF_3V3` brings VIO from ~25 mV to ~3.27 V, and main-CPU
GPIO 25 toggles at 2 s under that rail.

**Setup:** FreeWili 2 (RP2350B) over a CMSIS-DAP debug probe, interface 0
(display processor). Branch `master` @ 23810c8 + this change, SDK 2.3.0,
toolchain 14_2_Rel1. Main CPU running the stock FreeWili 2 firmware (OneWili
bridge). Nothing connected to the GPIO header.

## Where the design came from

The BSP had no VREF control at all. The reference implementation is
`fw2IOExpanderDisplay::setVREFConnection(fw2VREFConnection)` in the stock
firmware (`C:\~prj\fwt10\freewilimain\rmpLib\fw2IOExpanderDisplay.cpp:60`): it
clears all four VREF pins and asserts at most one. Pin indices come from the
`fw2IOExDisplayIOpin` enum in the matching header (index = expander bit, byte =
index/8), which agrees with `sensorview/src/platform/ioexp_pcal6524.h` for every
pin this change touches:

| Selection (`fw2VREFConnection`) | Expander pin | Port-2 bit | BSP name |
| --- | --- | --- | --- |
| `none` | — | — | `VREF_NONE` |
| `v3_3` | `P3V3_VREF` | 6 (0x40) | `VREF_3V3` |
| `v5_0` | `P5V_VREF` | 5 (0x20) | `VREF_5V0` |
| `vVIO` | `EXT_VREF` | 3 (0x08) | `VREF_EXT_PIN` |
| `vVREFInternal` | `INT_VREF` | 4 (0x10) | `VREF_PROG_VOUT` |

The stock firmware's "GPIO Voltage" panel (`fwAboutPanelVREF.cpp`) labels these
3.3V / 5.0 / Ext Pin / Prog Vout, and `Fw2Display.cpp:1072` boots the device at
`vVIO`.

The sweep below was run against a build where `ioexp_init()` left VREF
**disconnected**. On the owner's call, that default was then changed to
`VREF_EXT_PIN` to match the stock firmware, and re-verified — see "Follow-up"
at the end. Either way an app that drives the header at a known logic level must
call `ioexp_vref()`; `VREF_EXT_PIN` supplies nothing on a bare board.

## Measurement method

No scope was used. `rpADC::initFW2Display()` in the stock firmware shows the
*display* RP2350 has two ADC taps that `fwAboutPanelVREF` renders as "Vout" and
"Vio":

- GPIO 41 = ADC input 1 → programmable Vout monitor
- GPIO 45 = ADC input 5 → GPIO header VIO monitor

Both behind a 2:1 divider; the panel scales `raw/4095 * 2 * 3.3`. `hello_vref`
reads the same two inputs and prints integer millivolts (DIAG has no float
support). Neither GPIO is claimed by `bsp/platform/board.h`.

## What was run and what happened

`apps/hello_vref` sweeps every selection with a 200 ms settle, then settles on
3.3 V and toggles main-CPU GPIO 25 every 2 s via
`ow_io_gpio_set_io_toggle()`, reading the pin state back with
`ow_io_gpio_read_all()`. From `fw rtt -s 14`:

    ioexp: VREF select 0 -> P2 0x0
    hello_vref: VREF_NONE      -> VIO   25 mV, Vout 27 mV
    ioexp: VREF select 1 -> P2 0x40
    hello_vref: VREF_3V3       -> VIO 3304 mV, Vout 29 mV
    ioexp: VREF select 2 -> P2 0x20
    hello_vref: VREF_5V0       -> VIO 4841 mV, Vout 33 mV
    ioexp: VREF select 3 -> P2 0x8
    hello_vref: VREF_EXT_PIN   -> VIO 4814 mV, Vout 33 mV
    ioexp: VREF select 4 -> P2 0x10
    hello_vref: VREF_PROG_VOUT -> VIO   25 mV, Vout 27 mV
    ioexp: VREF select 0 -> P2 0x0
    hello_vref: VREF_NONE      -> VIO   25 mV, Vout 27 mV
    ioexp: VREF select 1 -> P2 0x40
    hello_vref: VREF_3V3       -> VIO 3271 mV, Vout 32 mV
    hello_vref: link up, toggling main-CPU GPIO 25 every 2000 ms
    hello_vref: GPIO 25 = 0 (bitfield 0xE0FF21F3, VIO 3275 mV)
    hello_vref: GPIO 25 = 1 (bitfield 0xE3FF21F3, VIO 3273 mV)
    hello_vref: GPIO 25 = 0 (bitfield 0xE1FF21F3, VIO 3278 mV)
    hello_vref: GPIO 25 = 1 (bitfield 0xE27F21F3, VIO 3276 mV)
    ... alternating cleanly for the rest of the capture

An earlier build without the sweep gave the same before/after at power-on:
`VREF_NONE -> VIO 301 mV` (the rail had not fully decayed after reset),
`VREF_3V3 -> VIO 3270 mV`.

## Verified behaviors

- **The bit mapping is right.** Each selection produced its own distinct
  Port-2 byte (`0x40 / 0x20 / 0x08 / 0x10`) and its own distinct VIO reading.
  A swapped pair would have shown up as two selections reading alike.
- **3.3 V is real and stable.** 3.27–3.30 V across the whole capture, and it
  did not sag while GPIO 25 switched.
- **Selections are mutually exclusive.** Every call clears all four bits first;
  moving 5V0 → EXT_PIN → PROG_VOUT never left a previous rail asserted (the
  `P2` values in the log are single-bit).
- **VREF_NONE really disconnects.** ~25 mV, i.e. the divider sitting at ground.
- **Round trip works.** NONE → 3V3 → … → NONE → 3V3 returned to the same
  reading, so the shadow byte is not accumulating state.
- **The toggle reaches the main CPU.** `ow_io_gpio_read_all()` bit 25 alternates
  0/1/0/1 in lockstep with the 2 s toggles — not merely "the command was
  accepted".
- **Other expander users are unaffected.** `ioexp_vref()` touches only the four
  VREF bits of the port-2 shadow; `ioexp: init ok` and the antenna default still
  appear, and the LCD/backlight came up normally on every flash.

## Not verified / open

- **The header pin voltage was never measured.** The ADC monitors the VIO *rail*,
  not GPIO 25's pad. That the level shifter passes 0 ↔ 3.3 V to the header pin
  is inferred, not measured — it needs a meter or scope on the header. The
  premise that the GPIO header is dead without VIO comes from the hardware
  owner, not from a measurement made here.
- **`VREF_EXT_PIN` read 4.81 V with nothing connected to the external
  Trig_IN/VREF pin.** That is suspiciously close to the 5 V rail reading
  (4.84 V) and was not chased down. Either the external pin floats near 5 V on
  this unit, or `EXT_VREF` does something other than what its name suggests when
  nothing drives the pin. Do not treat "EXT_PIN gives you the external pin's
  voltage" as confirmed until someone drives that pin to a known level.
- **`VREF_PROG_VOUT` read ~25 mV**, consistent with the programmable Vout being
  off (the Vout monitor read ~30 mV throughout). It was *not* verified against
  an enabled Vout — that needs `ow_io_analog_out_set_v_prog_vout()` on the main
  CPU first. So the INT_VREF bit is confirmed distinct, but its rail is
  untested under load.
- **The 5 V and external selections were only read, not exercised** with a load
  or with GPIO traffic.
- No host tests were added: `ioexp_vref()` is a two-line bit select against a
  hardware shadow, with no pure logic worth a `tests/` entry.

## Follow-up: `ioexp_init()` default changed to `VREF_EXT_PIN` (same day)

After the run above, the owner's call was to match the stock firmware rather
than boot disconnected, on the grounds that "if you use GPIO you must set VREF"
should be encoded in the repo rather than enforced by a dead-by-default rail.
`ioexp_init()` now sets `EXT_VREF` (P2 bit 3) and seeds the `s_vref` shadow;
`ioexp_vref_get()` was added so an app can query the selection.

Re-flashed and re-verified — the default is applied at init, not merely
declared:

    ioexp: init ok (LCD reset released; CC1101 + 433 MHz antenna; GPIO VREF = ext pin)
    hello_vref: after board_init: ioexp_vref_get() = 3, VIO 4809 mV, Vout 33 mV

4809 mV matches the 4798–4841 mV the explicit `VREF_EXT_PIN` sweep reads, so
init and `ioexp_vref(VREF_EXT_PIN)` land on the same state. The sweep and the
GPIO 25 toggles were unchanged by the new default.

Note this default is the selection with the unexplained reading (see "Not
verified / open"). It is the stock-firmware-compatible choice, not a
known-good logic level: on a bare board the external pin supplies nothing, so
apps driving the header must still pick a rail. `apps/toggleled` — which
toggled GPIO 25 with no VREF call at all and so could never have moved the
header pin — now calls `ioexp_vref(VREF_3V3)`.

The requirement is recorded in AGENTS.md invariant 11 (plus the "Gotchas for
automated edits" list), `docs/hardware/facts.md`, `docs/drivers/platform.md`,
and the header comment in `bsp/platform/ioexp.h`.

## Notes for the next person

- The `s_p2` shadow in `bsp/platform/ioexp.c` is now shared by IR power, USB D+
  and VREF. Any new port-2 user must read-modify-write it, as these do.
- `bsp/fw2.h` did not include `platform/ioexp.h` before this change (apps got
  the expander API only indirectly). It does now.
- Rev discrepancy, unchanged by this work and still unresolved: expander pin
  index 18 (P2_2) is `MCLR` (a hardware input) in
  `sensorview/src/platform/ioexp_pcal6524.h` and in this BSP's `ioexp_init()`
  (`cfg` port 2 = `0x04`), but `HP3_EN` (an output) in the stock firmware's
  `fw2IOExDisplayIOpin`. Nothing here depends on it.

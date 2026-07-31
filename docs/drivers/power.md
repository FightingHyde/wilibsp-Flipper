# Coprocessor power-zone protocol

The board's power coprocessor sequences every switched rail. Rails
outside its boot subset stay off until explicitly requested — notably the
audio codec rail (zone 3) and the CAN rail (zone 15). The display CPU
requests rails over the same UART link that carries the coprocessor's
status/button frames (UART1, 62500 baud, 8N1, both directions). The
display CPU is the only device that drives the coprocessor's receive
line; there is no alternate control path.

BSP support: `bsp/input/uartkbd.*` owns the link (RX parsing and command
TX); `bsp/input/picpwr_frame.*` is the pure frame/mask logic
(host-tested); `bsp/input/picpwr.*` is the application-facing API.

## Command frame

One shape in both directions:

```
SYNC | ID | payload[N] | CHK
```

- `SYNC` — one byte, `0xB0` for commands and replies.
- `ID` — one byte; **payload length is implied by the id**. There is no
  length field. Emitting the wrong payload size lands the checksum in the
  wrong position and the frame is discarded.
- `CHK` — 8-bit additive sum (wraparound) of every preceding byte,
  including `SYNC` and `ID`. Not a CRC. No escaping — payload bytes may
  legitimately equal a sync value.

The unsolicited status frame differs: sync `0xBD`, id `0x1D`, 20-byte
payload, same additive checksum.

## Per-frame handshake (mandatory)

The coprocessor's receiver is command-gated; skipping this sequence means
the bytes are silently ignored:

1. Assert a UART break for ~2 ms, then release it.
2. Wait for a bare (unframed) activity byte `0xC9`, with a ~500 ms
   fallback timeout — proceed anyway if it never arrives.
3. Settle ~5 ms, transmit the frame, then allow ~50 ms before treating
   the link as free.

`0xC9` may arrive mid-frame from the RX parser's perspective. It must be
intercepted ahead of the parser and any partially collected frame
dropped (implemented inside `uartkbd_cmd_send()`).

## Power-zone command (`ID 0x00`, payload 8 bytes)

| Offset | Size | Field |
|---|---|---|
| 0–2 | 3 | awake mask — rails enabled now |
| 3–5 | 3 | sleep mask — rails kept through sleep |
| 6 | 1 | wake-source field |
| 7 | 1 | wake-source overflow field |

This is a single **atomic write of all four fields** — no partial form,
no read-modify-write on the device, and the sleep/wake fields cannot be
read back. Consequences:

- Always send a **complete awake mask**: any zone bit left clear switches
  that rail off (including the display rail).
- Keep a cached copy of all four fields and send the full set every time.
- Seed the awake mask's rail bits from **live status-frame state** (see
  below), never from a local cache alone — other actors move rails
  without notice.

Worked example — zones 1–17 on, sleep/wake zeroed:

```
B0 00 01 FF FF 00 00 00 00 00 AF
```

(Checksum: the byte sum is 0x2AF; truncated to 8 bits = 0xAF.) Note the
reserved bits above zone 17 are zero — that is the required form, not a
simplification. Suitable as a smoke test only because it zeroes the sleep
configuration.

## Zone mask encoding

24-bit field, most-significant byte first. Zone *N* is bit *N−1*, so
zone 1 is bit 0 of the **last** byte and zone 17 is bit 0 of the first.

- Zones 1–17 are switched rails (zone 9 excepted — a flag, see the map).
- Bits above zone 17 are **RESERVED — MUST BE ZERO.** They reach board
  control lines rather than switched rails, are not safely
  software-controllable on current hardware revisions, and zero matches
  the boot state. They are deliberately **not** exposed by the `picpwr`
  API — it clamps every outgoing mask to zones 1–17. If software control
  of one is ever needed it must be a separate, explicitly documented,
  opt-in call gated on a board revision that supports it.

## Zone map

| Zone | Powers | On at boot? | Application notes |
|---|---|---|---|
| 1 | Motion/magnetic/humidity/light sensors + an I/O expander | yes | The expander serves more than the sensors — switching off has a wide blast radius |
| 2 | LCD panel + touch controller | yes | Off = blank screen, no touch |
| 3 | Audio codec | no | — |
| 4 | Sub-GHz radio + LoRa module (shared rail) | no | The sub-GHz radio is usable; the LoRa module is held in reset by a reserved control line and is **not** usable through this interface |
| 5 | Wi-Fi / Bluetooth module | no | — |
| 6 | FPGA + its external memory | no | — |
| 7 | microSD card + bridge | yes | Unmount before switching off — corruption risk |
| 8 | USB hub | with USB cable | Managed automatically by the device on cable attach/detach — do not fight it |
| 9 | *Flag, not a rail*: status-LED enable | set | When set, the device's status LED shows heartbeat/activity and the application CPUs mirror it. Clear for LED-quiet operation. Safe either way; no rail moves |
| 10 | Addressable RGB LEDs | no | — |
| 11 | Analog subsystem (DAC, ADC, op-amps) | no | — |
| 12 | No confirmed load | no | Leave in its default state. Verified not required for the LCD (apps draw with it off) or the video output (signal, sink detection and hotplug all unaffected with it off) |
| 13 | NFC + low-frequency RFID | no | — |
| 14 | USB-serial bridge | with USB cable | Managed automatically on cable attach/detach |
| 15 | CAN controller + transceiver | no | — |
| 16 | On-board debug probe | with USB cable | Off mid-session = you lose your debugger. Managed automatically on cable attach/detach |
| 17 | Compute module + its microSD | no | **Not usable through this interface**: its run/enable line is a reserved control bit (held clear). No safe-shutdown path exists — never cut power to a running module |

Notes on the map:

- **The boot-on set is a property of the device firmware and may change
  between versions. Do not rely on it** — request the zones your
  application needs explicitly, every time.
- **Omitting a zone from the awake mask switches it off.** The pattern
  that prevents blanking your own display on the first attempt: read
  live rail state, OR in the zones you need, send the complete mask
  (`picpwr_keep_awake()` / `picpwr_ensure_awake()` do this for you).
- The display CPU's own rail is not in the mask at all; it cannot be
  reached — and therefore cannot be switched off by accident.
- Nothing in 1–17 is electrically unsafe to switch off. The real risks
  are data loss (zones 7 and 17) and losing the interface you are
  working through (zones 2, 8, 16).
- Whether a zone's peripheral is usable from a display-CPU application
  is a bus-topology question this document does not settle. The
  checkable rule: if this BSP ships a driver for the peripheral, the
  zone is usable from here; if it does not, treat it as not supported
  yet rather than as a hardware limitation.

## Reading rail state back

Rail state is confirmed from the **unsolicited status frame** (the same
one carrying buttons/charger data), not from any cache. In its 20-byte
payload, high = powered:

| Zone | Byte | Mask | | Zone | Byte | Mask |
|---|---|---|---|---|---|---|
| 1 | 3 | 0x08 | | 10 | 5 | 0x08 |
| 2 | 3 | 0x10 | | 11 | 5 | 0x10 |
| 3 | 3 | 0x20 | | 12 | 5 | 0x20 |
| 4 | 3 | 0x40 | | 13 | 6 | 0x01 |
| 5 | 3 | 0x80 | | 14 | 6 | 0x02 |
| 6 | 4 | 0x01 | | 15 | 6 | 0x04 |
| 7 | 4 | 0x02 | | 16 | 6 | 0x10 |
| 8 | 5 | 0x02 | | 17 | 7 | 0x08 |
| 9 | 5 | 0x04 | | | | |

Byte indices are into the 20-byte payload (frame bytes 2–21). The status
frame also carries indications for the reserved control lines (byte 6
`0x08`, byte 7 `0x01`, byte 5 `0x80`). **Exclude them — and everything
above zone 17 — from any requested-vs-actual comparison**: one of those
pins is set high by the coprocessor's own boot code but never written by
the rail walk, so a command correctly sending 0 reads back 1 forever. A
comparison that includes it mismatches permanently and, if wired to a
debounced re-assert, thrashes the link with rail walks. Compare zones
1–17 only.

## Timing and serialization

Applying a mask runs a **staggered rail walk with roughly one second of
inrush delay per pass, during which the coprocessor does not parse the
link**. Rules:

- Never send frames back-to-back; allow well over one second between
  commands (`picpwr_send()` enforces 1.5 s) and never stack retries.
- A mask is applied unconditionally even when nothing changed — every
  send costs a walk.
- Do not judge success until the walk completes; then confirm via the
  status frame.

## Autonomous rail changes

The coprocessor changes rails on its own; caches go stale:

- USB detach/attach clears/restores the USB-bridge and debug-probe rails.
- Sleep applies the sleep mask; wake re-applies the coprocessor's cached
  awake mask.
- Low-battery, long-button-hold and ship-mode shutdowns drop everything;
  a coprocessor watchdog reset returns to the boot subset.

There is no keepalive and no idle timeout. Applications that depend on a
rail should watch the status frame and re-assert (debounced — never
during a walk) if their rail reads off. `picpwr_keep_awake()` +
`picpwr_task()` implement exactly this.

## Usage guidance

- Request rails **before** initializing the peripherals that sit on
  them: a rail transition can disturb devices already operating on a
  shared bus, and a peripheral initialized before its rail is stable
  reads back garbage.
- After a rail walk completes, re-initialize shared-bus state (e.g. run
  I2C bus recovery) before first use if devices on that bus were live
  during the walk.
- Status frames may be absent for several seconds after the display CPU
  resets; tolerate that rather than treating it as a dead link.
- Never seed an awake mask from a single status frame: use two agreeing
  frames (as `picpwr_ensure_awake()` does) or send a superset. A stale
  snapshot echoed back switches off every rail it failed to report.

## Out-of-band bytes

Bare, unframed single bytes appear between and even inside frames; the
parser tolerates them anywhere:

| Byte | Meaning |
|---|---|
| `0xC9` | RX active (handshake gate) |
| `0xE5` | frame parsed, checksum OK |
| `0xE4` | checksum mismatch |
| `0xE2` | unrecognized command id |
| `0xE1` | first byte was not the command sync |

`0xE5` acknowledges the parse only — rails have not necessarily finished
moving. There is no dedicated reply to the power-zone command; confirm
via the status frame.

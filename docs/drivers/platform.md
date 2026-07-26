# Platform driver (`bsp/platform/`)

**What it does:** brings the board to a known-good state before any other
driver runs — vreg + clocks (250 MHz, `clk_peri` re-sourced), the CC1101 CS
parked HIGH, the backlight GPIO initialized off, I2C1 up, and the PCAL6524
I/O expander configured (LCD reset release, antenna select, SPI1 buffer
directions). Also provides PSRAM bring-up and the shared-SPI1 arbitration
primitives. Diagnostics go out over SEGGER RTT (`DIAG()`).

**How to use it:** call `board_init()` once at the top of `main()`, before
any display/touch/LED calls. It's the very first thing every app does:

```c
#include "fw2.h"
#include "platform/diag.h"

int main(void) {
    board_init();               // vreg, 250 MHz clocks + clk_peri re-source,
                                 // CC1101 CS parked, backlight off, I2C1 up,
                                 // I/O expander (LCD reset release, antenna)
    st7796_init();
    board_backlight_set(1);      // turn the backlight on once the panel is ready
    ...
    DIAG("hello_display up: sys=%u kHz\n", BOARD_SYS_CLOCK_KHZ);
}
```

**Key APIs** (`bsp/platform/board.h`): `board_init()`,
`board_backlight_set(uint8_t level)`, `board_i2c1_init()` (called by
`board_init()`, exposed in case a driver needs to re-init I2C1 standalone).
`bsp/platform/diag.h`: `DIAG(...)` → RTT channel 0 (`fw rtt` to view; no
floats). `bsp/platform/psram.h`: `psram_init()` / `psram_selftest()` for the
8 MB APS6404L — a thin shim over the SDK's `hardware_psram` (see the PSRAM
section below). `bsp/platform/ioexp.h`: `ioexp_init()` / `ioexp_antenna()`
(PCAL6524, I2C1 addr 0x23). `bsp/platform/spi_bus.h`:
`spi_bus_acquire_cc1101()` / `_release()` / `_cs()` — arbitration for the
shared SPI1 bus, ready for when the CC1101 driver is harvested.

## PSRAM (8 MB APS6404L)

Since SDK 2.3.0 the SDK owns PSRAM. `bsp/boards/freewili2.h` declares
`PICO_PSRAM_CS_PIN 47` and `PICO_PSRAM_SIZE_BYTES (8 * 1024 * 1024)`, and
`hardware_psram`'s `runtime_init` hook brings the chip up and maps it at
`PSRAM_BASE` (`0x11000000`) **before `main()` runs**. There is no bring-up call
to make: `psram_init()` now just reports the size.

**Allocate with the linker, never by casting `PSRAM_BASE`.**
`PICO_PSRAM_SIZE_BYTES` also sizes the linker's PSRAM region, whose `ORIGIN` is
`0x11000000` — the same address `PSRAM_BASE` points at. So a raw
`(uint16_t *)PSRAM_BASE` pointer silently aliases whatever the linker allocated
there. Use the SDK's section macros:

```c
static uint16_t __uninitialized_psram("fb") s_fb[ST7796_W * ST7796_H];
```

`__in_psram("group")` is the initialised variant (copied from flash at boot);
`__uninitialized_psram("group")` is the NOLOAD variant you want for buffers.
`apps/hello_keyboard` and `apps/hello_charger` are the worked examples.

**`arm-none-eabi-size` over-reports RAM.** The `.psram_noload` section is NOBITS,
so the flat `size` output folds it into `bss` — `hello_keyboard` reads as 311 KB
of bss when its real SRAM `.bss` is 4 KB. Use `size -A` and read `.bss` (SRAM)
separately from `.psram_noload` (PSRAM) when checking the 512 KB SRAM budget.

**Timing is re-applied after the overclock.** Boot-time init runs at the boot
`clk_sys`, and nothing in `hardware_clocks` re-tunes PSRAM when the clock
changes, so `board_init_clk()` calls `psram_configure_params()` **and**
`psram_reinitialize()` after `set_sys_clock_khz()`. Both are required — see
`docs/hardware/facts.md`.

**Dependencies:** none within the BSP (this is the foundation layer). Pulls
in Pico SDK `hardware_clocks`, `hardware_vreg`, `hardware_i2c`,
`hardware_gpio`, `hardware_psram` and the vendored SEGGER RTT
(`bsp/third_party/segger_rtt/`).

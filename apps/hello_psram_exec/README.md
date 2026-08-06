# hello_psram_exec

Minimal freestanding example of a DISPLAY app whose executable image lives in
PSRAM while its bootstrap executes from normal SRAM. It intentionally does not
use RP2350 BOOTRAM.

This is not a cold-boot firmware image. It is for the DISPLAY recovery loader,
which must initialize QMI/PSRAM and copy the UF2 image into PSRAM before jumping
to its vector table. Do not flash this ELF over the stock DISPLAY firmware.

## What demonstrates the split

- 'psram_exec.ld' places the vector table, reset stub, 'psram_main()', and
  ordinary code at 0x11000000..0x11800000.
- The vector's initial stack points into SRAM.
- 'startup.S' copies '.sram_bootstrap' from its PSRAM load address to its SRAM
  execution address and branches there.
- 'bootstrap.c' initializes data/BSS, installs the PSRAM vector table, and
  enters 'psram_main()' without rerunning cold-boot initialization.
- A post-link checker fails the build if symbols land in the wrong regions.
- 'make_uf2.py' emits PSRAM-addressed UF2 blocks because the SDK's normal
  cold-boot UF2 conversion path does not accept a PSRAM entry point.

The PSRAM-resident workload pulses the LCD backlight. GPIO 25 is explicitly
the display CPU's 'PIN_LCD_BL', not external header GPIO 25.

Build with 'fw build hello_psram_exec'. Install the UF2 through
'fw install-app'; it rejects flash targets and mixed SRAM/PSRAM payloads.

## Deliberate limitations

The example is freestanding: no SDK CRT, BSP, interrupts, C library, RTT, or
clock changes. Adding those requires auditing their startup assumptions. Any
clock or QMI-sensitive function and all of its callees must execute from SRAM.
The build proves placement only; hardware behavior requires a loader run and
an observable backlight pulse.

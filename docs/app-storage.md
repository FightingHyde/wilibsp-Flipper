# App files on the SD card

FreeWili loadable apps belong in `/apps/` as `.uf2` files. Install one from a
connected development machine with:

```bash
fw install-app build/apps/my_app/my_app.uf2
```

Organize larger collections with a relative subfolder under `/apps`:

```bash
fw install-app my_app.uf2 --folder beta/radio
```

This installs `/apps/beta/radio/my_app.uf2`. The command creates missing
directories and rejects absolute paths, backslashes, empty components, and
`.`/`..` components before it touches the device.

The command finds MAIN with fwFinder, asks MAIN to hand the SD card to the USB
reader, waits for the drive to mount, copies the UF2 into `/apps/`, safely
unmounts it, and returns the card to MAIN. Use `--device SERIAL` when more than
one FreeWili is connected. `--port COM44` (or the corresponding POSIX device)
is an explicit fallback for legacy USB identities fwFinder cannot recognize.
The host needs the `pyfwfinder` and `pyserial`
Python packages; the command reports either missing dependency directly.

Before touching the device, `fw install-app` parses every UF2 block and refuses
anything targeting QSPI flash. Loadable DISPLAY apps must target SRAM
(`0x20000000..0x20070000`) or PSRAM (`0x11000000..0x11800000`) consistently.
This prevents an app from replacing the stock DISPLAY firmware. DISPLAY's
recovery loader itself is immutable OTP code, not a flash-resident region;
firmware replacement is an explicit maintenance workflow, not app installation.

## Publishing apps

The loadable `.uf2` is part of the FreeWili app contract. App repositories
must attach the validated UF2 as a downloadable release artifact for every
published app release. A source tag or a short-lived CI artifact alone is not
enough: testers and users must be able to download that exact release UF2
without reproducing the embedded toolchain locally.

## PSRAM-resident app startup

An app whose executable image lives in PSRAM needs a small **SRAM bootstrap**.
Do not call this BOOTRAM: RP2350 BOOTRAM is special memory used by the boot ROM
and is not the application's general-purpose startup region.

The DISPLAY app loader initializes QMI CS1 and fills the PSRAM window before it
jumps to the app. From that point onward, code fetched from PSRAM depends on
that QMI configuration remaining valid. A normal cold-boot CRT may reset boot
ROM state, peripherals, clocks, or QMI-related state; running those steps from
PSRAM can invalidate the bus carrying the next instruction and leave the
display black.

For a PSRAM-resident app:

- Keep the vector table at the beginning of the PSRAM image. Its initial stack
  pointer must point into SRAM.
- Put the C/C++ runtime entry and any clock/QMI-sensitive boot routines in
  SRAM. A minimal reset handler may begin in PSRAM only long enough to copy
  this bootstrap into SRAM and enter it.
- Treat PSRAM as already initialized by the loader. Do not run the SDK's
  cold-boot PSRAM setup again while executing from that same PSRAM window.
- Skip or replace cold-boot reset and clock initializers that would disturb the
  inherited loader state. Initialize ordinary application peripherals after
  the SRAM bootstrap takes control.
- Perform every clock transition and PSRAM timing update from SRAM. Adjust the
  QMI timing before raising the system clock so PSRAM never exceeds its bus
  limit during the transition.

These requirements apply to apps **executing from PSRAM**, not to ordinary BSP
apps linked and loaded directly into SRAM with the `no_flash` binary type.

Before publishing a PSRAM app, make the build verify all of the following:

1. The first vector-table words contain an SRAM stack pointer and a PSRAM reset
   address.
2. The runtime entry and clock/QMI-sensitive startup symbols resolve inside
   SRAM (`0x20000000..0x20070000`), not PSRAM.
3. Every UF2 payload block targets PSRAM (`0x11000000..0x11800000`) and none
   targets QSPI flash. `fw install-app` enforces the UF2 portion before copying.
4. On hardware, the loader reports success and the app reaches an observable
   runtime milestone such as display output, USB enumeration, or diagnostics.

## App-owned data

Apps should normally keep their saved data under `/appdata/<app-name>/`; for
example, Meshtastic uses `/appdata/meshtastic/`. Keeping maps, logs,
preferences, and other app-owned files there makes the card root easier to
navigate and makes ownership clear.

This is a convention for convenience, not a filesystem restriction. An app
may use the root or another location when that is genuinely useful to the
user. Do not hide user-authored files inside `/appdata/` merely to satisfy the
convention.

# PSRAM bootstrap interrupt masking — hardware findings

## Scope

Investigated PSRAM-targeted apps that launched to a black screen and did not
respond to the HOME recovery hold.

## Failure found

The SRAM bootstrap enabled interrupts before changing the system clocks and
re-timing QMI/PSRAM. The app vector table and ordinary interrupt handlers are
in PSRAM, so an interrupt in that interval could fetch code from the bus while
it was being reconfigured and lock the display CPU before `main()`.

The bootstrap now preserves the loader's interrupt-disabled state through
`board_init_psram()` and enables interrupts only after QMI re-timing completes.
`board_init()` also recognizes that bootstrap initialization already occurred
and performs only inherited-peripheral initialization when the app calls it.

## Observed result

After installing and launching the rebuilt `hello_psram_exec` image, an RTT
capture reported:

    psram: bootstrap copied
    board: spi
    board: radio cs
    board: backlight
    board: i2c
    board: ioexp
    board: peripherals ready

This proves that the PSRAM image passed the previously dead startup interval
and reached the SRAM BSP bootstrap and peripheral initialization on hardware.

## Remaining verification

The GuitarMan and Orca Field Notes images were rebuilt against the fix, but
could not be copied to the card during this session because the USB SD reader
failed to enumerate. Their LCD, touch, About, and five-second HOME behavior
remain unverified on hardware. A later launcher attempt also reported the
installed PSRAM example path as missing or invalid; the card contents need to
be refreshed before the next runtime test.

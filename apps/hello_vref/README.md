# hello_vref

Smoke test for the user-GPIO reference voltage (VIO) select on the display
I/O expander.

The app reports what `board_init()` left VREF at, sweeps all five selections
printing both rail monitors, settles on `VREF_3V3`, then toggles **main-CPU
GPIO 25** every 2 s over the FwGUI OneWili link (as `apps/toggleled` does at
500 ms) and reads the pin state back.

    fw build hello_vref && fw flash hello_vref
    fw rtt -s 12

Expected:

    ioexp: init ok (... GPIO VREF = ext pin)
    hello_vref: after board_init: ioexp_vref_get() = 3, VIO 4809 mV, Vout 33 mV
    hello_vref: VREF_NONE      -> VIO   17 mV, ...
    hello_vref: VREF_3V3       -> VIO 3260 mV, ...
    hello_vref: VREF_5V0       -> VIO 4841 mV, ...
    ...
    hello_vref: GPIO 25 = 0 (bitfield 0xE0FF21F3, VIO 3275 mV)
    hello_vref: GPIO 25 = 1 (bitfield 0xE3FF21F3, VIO 3278 mV)

On a scope/DMM on header GPIO 25: a 0 V ↔ 3.3 V square wave, 4 s period. With
`VREF_NONE` those same toggles still log `GPIO 25 = 0/1` while the header pin
does not move — that silent failure is the point of the test.

Rail readings come from the display CPU's own monitors (GPIO 45 = VIO,
GPIO 41 = Vout, 2:1 divider), so no external meter is needed to see the rail
switch.

The main CPU must be running the stock FreeWili 2 firmware (OneWili bridge).

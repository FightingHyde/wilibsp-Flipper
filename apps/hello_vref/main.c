/* hello_vref — connect the user-GPIO reference voltage (VIO) to the internal
 * 3.3 V rail via the display I/O expander, then toggle MAIN-CPU GPIO 25 every
 * 2 s over the FwGUI link so the level shifter has something to drive.
 *
 * Without ioexp_vref() the header pin stays put no matter what the main CPU
 * does: the GPIO level shifters have no rail.
 *
 * To make that visible without a scope, the app also reads the display CPU's
 * own rail monitors and prints them: VIO on GPIO45 (ADC input 5) and the
 * programmable Vout on GPIO41 (ADC input 1), each behind a 2:1 divider. Pin
 * assignment + scaling from rpADC::initFW2Display() and fwAboutPanelVREF.cpp
 * in the stock FreeWili 2 firmware. It logs VIO with VREF disconnected first,
 * then after selecting 3.3 V, so the log carries its own before/after.
 *
 * The main CPU must be running the stock FreeWili 2 firmware (it carries the
 * OneWili bridge). See libs/onewili/README.md. */
#include "fw2.h"
#include "platform/diag.h"
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "onewili.h"
#include "onewili_fwgui.h"

#define TOGGLE_PIN       25
#define TOGGLE_PERIOD_MS 2000

#define PIN_VOUT_SENSE 41   // ADC input 1: programmable Vout monitor
#define PIN_VIO_SENSE  45   // ADC input 5: GPIO header VIO monitor
#define ADC_IN_VOUT    1
#define ADC_IN_VIO     5

static void rail_monitor_init(void) {
    adc_init();
    adc_gpio_init(PIN_VOUT_SENSE);
    adc_gpio_init(PIN_VIO_SENSE);
}

/* Millivolts at the rail: 12-bit result, 3.3 V ADC reference, 2:1 divider.
   DIAG() has no float support, so keep it integer. */
static uint32_t rail_mv(uint32_t adc_input) {
    adc_select_input(adc_input);
    return (uint32_t)adc_read() * 6600u / 4095u;
}

int main(void) {
    board_init();   /* must precede ow_open_fwgui: uart_init reads clk_peri */
    rail_monitor_init();

    /* What board_init() left us with, before anything here touches VREF. */
    DIAG("hello_vref: after board_init: ioexp_vref_get() = %d, VIO %u mV, Vout %u mV\n",
         (int)ioexp_vref_get(), rail_mv(ADC_IN_VIO), rail_mv(ADC_IN_VOUT));

    /* Sweep every selection once so the log shows each expander bit doing
       something distinct, then settle on 3.3 V. */
    static const struct { uint8_t sel; const char *name; } sweep[] = {
        { VREF_NONE,       "NONE     " },
        { VREF_3V3,        "3V3      " },
        { VREF_5V0,        "5V0      " },
        { VREF_EXT_PIN,    "EXT_PIN  " },
        { VREF_PROG_VOUT,  "PROG_VOUT" },
        { VREF_NONE,       "NONE     " },
        { VREF_3V3,        "3V3      " },   /* final state for the toggle test */
    };
    for (unsigned i = 0; i < count_of(sweep); i++) {
        ioexp_vref(sweep[i].sel);
        sleep_ms(200);   /* let the rail settle before measuring */
        DIAG("hello_vref: VREF_%s -> VIO %u mV, Vout %u mV\n",
             sweep[i].name, rail_mv(ADC_IN_VIO), rail_mv(ADC_IN_VOUT));
    }

    static ow_device dev;   /* ~37 KB of buffers - far too big for the 2 KB stack */
    while (ow_open_fwgui(&dev) != OW_OK) {
        DIAG("hello_vref: FwGUI link open failed (is the main CPU running stock fw?), retry in 1 s\n");
        sleep_ms(1000);
    }
    DIAG("hello_vref: link up, toggling main-CPU GPIO %d every %d ms\n",
         TOGGLE_PIN, TOGGLE_PERIOD_MS);

    for (;;) {
        ow_status s = ow_io_gpio_set_io_toggle(&dev, TOGGLE_PIN);
        if (s != OW_OK) {
            DIAG("hello_vref: toggle failed, status %d\n", (int)s);
        } else {
            /* Read the main CPU's GPIO bitfield back so the log shows the pin
               actually moved, rather than just that the command was accepted. */
            uint32_t gpios = 0;
            ow_status r = ow_io_gpio_read_all(&dev, &gpios);
            if (r != OW_OK)
                DIAG("hello_vref: GPIO %d toggled, read_all failed (status %d), VIO %u mV\n",
                     TOGGLE_PIN, (int)r, rail_mv(ADC_IN_VIO));
            else
                DIAG("hello_vref: GPIO %d = %d (bitfield 0x%x, VIO %u mV)\n",
                     TOGGLE_PIN, (gpios >> TOGGLE_PIN) & 1u, gpios, rail_mv(ADC_IN_VIO));
        }
        sleep_ms(TOGGLE_PERIOD_MS);
    }
}

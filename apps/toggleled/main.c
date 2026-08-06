/* toggleled — blink MAIN-CPU GPIO 25 from the display CPU over the FwGUI
 * link (UART0 @ 8 Mbaud, HW flow control on GPIO 0-3). The main CPU must
 * run the stock FreeWili 2 firmware, which carries the OneWili bridge.
 *
 * NOTE the ioexp_vref() call below: the GPIO header is level shifted and the
 * pin will not move without a VIO rail, however happily the toggles report
 * OK. ioexp_init() defaults VIO to the external Trig_IN/VREF pin, which is
 * nothing at all on a bare board — so a demo that wants a visible 3.3 V
 * square wave has to say so. See bsp/platform/ioexp.h. */
#include "fw2.h"
#include "platform/diag.h"
#include "pico/stdlib.h"
#include "onewili.h"
#include "onewili_fwgui.h"
#include "input/app_recovery_onewili.h"

int main(void) {
    board_init();   /* must precede ow_open_fwgui: uart_init reads clk_peri */
    fw2_app_recovery_init();
    st7796_init();
    st7796_fill_screen(0x0000);
    board_backlight_set(1);
    st7796_draw_text(8, 8, 2, 0xFFFF, 0x0000, "ONEWILI GPIO TEST");
    st7796_draw_text(8, 40, 1, 0xFFFF, 0x0000, "HEADER VIO = 3.3V");
    st7796_draw_text(8, 56, 1, 0xFFFF, 0x0000, "TOGGLING GPIO 25");
    st7796_draw_text(8, 288, 1, 0xFFFF, 0x0000, "HOLD HOME 5S TO EXIT");
    ioexp_vref(VREF_3V3);   /* required: no VIO rail => the header pin cannot drive */

    static ow_device dev;   /* ~37 KB of buffers - far too big for the 2 KB stack */
    /* ow_open_fwgui currently cannot fail (it only sends the reset byte,
     * no handshake) - this loop future-proofs a smarter open. A missing
     * bridge surfaces later as toggle timeouts on the DIAG below. */
    while (fw2_app_recovery_open_onewili(&dev) != OW_OK) {
        fw2_app_recovery_task();
        DIAG("toggleled: FwGUI link open failed (is the main CPU running stock fw?), retry in 1 s\n");
        fw2_app_recovery_sleep_ms(1000);
    }
    DIAG("toggleled: link up, toggling main-CPU GPIO 25 every 500 ms\n");

    for (;;) {
        fw2_app_recovery_task();
        ow_status s = ow_io_gpio_set_io_toggle(&dev, 25);
        if (s != OW_OK) {
            DIAG("toggleled: toggle failed, status %d\n", (int)s);
            st7796_fill_rect(8, 96, 360, 16, 0x0000);
            st7796_draw_text(8, 96, 2, 0xFFFF, 0x00F8, "TOGGLE FAILED");
        } else {
            static bool high;
            high = !high;
            st7796_fill_rect(8, 96, 360, 16, 0x0000);
            st7796_draw_text(8, 96, 2, 0xFFFF, 0x0000,
                             high ? "GPIO 25 TOGGLED HIGH" : "GPIO 25 TOGGLED LOW");
        }
        fw2_app_recovery_sleep_ms(500);
    }
}

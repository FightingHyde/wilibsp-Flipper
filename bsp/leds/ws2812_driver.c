// src/leds/ws2812_driver.c — adapted from evaderkrub/sensorview (BSD-3-Clause).
#include "leds/ws2812_driver.h"
#include "leds/led_color.h"
#include "ws2812.pio.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"

static PIO     s_pio;
static uint    s_sm;
static rgb_t   s_fb[WS2812_NUM_PIXELS];
static uint8_t s_brightness = 32;   // USB-safe default

void ws2812_init(PIO pio, uint sm, uint gpio) {
    s_pio = pio; s_sm = sm;
    uint offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, sm, offset, gpio, 800000.0f, false);
    gpio_set_outover(gpio, GPIO_OVERRIDE_INVERT);   // board LED data line is inverted
    ws2812_clear();
    ws2812_show();
}

void ws2812_set_pixel(uint i, rgb_t c) { if (i < WS2812_NUM_PIXELS) s_fb[i] = c; }
void ws2812_fill(rgb_t c) { for (uint i = 0; i < WS2812_NUM_PIXELS; i++) s_fb[i] = c; }
void ws2812_clear(void) { rgb_t z = {0, 0, 0}; ws2812_fill(z); }
void ws2812_set_brightness(uint8_t level) { s_brightness = level; }
uint8_t ws2812_get_brightness(void) { return s_brightness; }

void ws2812_show(void) {
    for (uint i = 0; i < WS2812_NUM_PIXELS; i++) {
        rgb_t s = { led_color_scale(s_fb[i].r, s_brightness),
                    led_color_scale(s_fb[i].g, s_brightness),
                    led_color_scale(s_fb[i].b, s_brightness) };
        pio_sm_put_blocking(s_pio, s_sm, led_color_pack_grb(s) << 8u);  // left-justify 24 bits
    }
    sleep_us(300);   // WS2812 reset/latch (>= 280 us)
}

static void temporary_zero_frame(PIO pio, uint sm) {
    for (uint i = 0; i < WS2812_NUM_PIXELS; i++)
        pio_sm_put_blocking(pio, sm, 0);
    /* put_blocking() only waits for FIFO space. Drain it, allow the final
     * 24-bit word to leave the OSR, then provide the >=280 us latch time. */
    while (!pio_sm_is_tx_fifo_empty(pio, sm)) tight_loop_contents();
    sleep_us(330);
}

void ws2812_clear_once(PIO pio, uint gpio) {
    uint sm = (uint)pio_claim_unused_sm(pio, true);
    uint offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, sm, offset, gpio, 800000.0f, false);
    gpio_set_outover(gpio, GPIO_OVERRIDE_INVERT);

    /* The first frame after starting this strip is hardware-known not to
     * reach every pixel reliably. The second establishes OFF everywhere. */
    temporary_zero_frame(pio, sm);
    temporary_zero_frame(pio, sm);

    pio_sm_set_enabled(pio, sm, false);
    pio_sm_clear_fifos(pio, sm);
    pio_sm_restart(pio, sm);
    pio_remove_program(pio, &ws2812_program, offset);
    pio_sm_unclaim(pio, sm);
}

#include <stdint.h>

/* RP2350 registers for internal display-CPU GPIO 25 (PIN_LCD_BL). */
#define IO_BANK0_GPIO25_CTRL (*(volatile uint32_t *)0x400280ccu)
#define PADS_BANK0_GPIO25    (*(volatile uint32_t *)0x40038068u)
#define SIO_GPIO_OUT_SET     (*(volatile uint32_t *)0xd0000018u)
#define SIO_GPIO_OUT_CLR     (*(volatile uint32_t *)0xd0000020u)
#define SIO_GPIO_OE_SET      (*(volatile uint32_t *)0xd0000038u)
#define LCD_BL_MASK          (1u << 25)

static volatile uint32_t s_psram_heartbeat;

__attribute__((noinline))
static void psram_delay(void) {
    for (volatile uint32_t i = 0; i < 12000000u; ++i)
        __asm volatile ("nop");
}

/* This function and psram_delay execute directly from PSRAM. */
__attribute__((noreturn))
void psram_main(void) {
    PADS_BANK0_GPIO25 &= ~(1u << 8); /* release RP2350 pad isolation */
    IO_BANK0_GPIO25_CTRL = 5u;       /* GPIO_FUNC_SIO */
    SIO_GPIO_OE_SET = LCD_BL_MASK;

    for (;;) {
        SIO_GPIO_OUT_SET = LCD_BL_MASK;
        psram_delay();
        SIO_GPIO_OUT_CLR = LCD_BL_MASK;
        psram_delay();
        ++s_psram_heartbeat;
    }
}

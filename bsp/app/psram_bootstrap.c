#include <stdint.h>
#include "platform/diag.h"

extern uint32_t __data_load_source__, __data_start__, __data_end__;
extern uint32_t __bss_start__, __bss_end__;
extern uint32_t __vectors;
extern void board_init_psram(void);
extern int main(void);

__attribute__((section(".sram_bootstrap"), noreturn, used,
               optimize("no-tree-loop-distribute-patterns")))
void fw2_psram_bootstrap(void) {
    volatile uint32_t *src = &__data_load_source__;
    for (volatile uint32_t *dst = &__data_start__; dst < &__data_end__;)
        *dst++ = *src++;
    for (volatile uint32_t *dst = &__bss_start__; dst < &__bss_end__;)
        *dst++ = 0;
    *(volatile uint32_t *)0xe000ed08u = (uint32_t)(uintptr_t)&__vectors;
    DIAG("psram: bootstrap copied\n");
    /* The second-stage loader enters with interrupts disabled.  Keep them
     * disabled while board_init_psram() changes clocks and re-times QMI: the
     * vector table and normal handlers live in PSRAM, so servicing an IRQ in
     * that window would fetch instructions from the bus being reconfigured. */
    board_init_psram();
    __asm volatile("cpsie i" ::: "memory");
    (void)main();
    for (;;) {}
}

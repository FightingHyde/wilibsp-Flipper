#include <stdint.h>

extern uint32_t __data_load_source__, __data_start__, __data_end__;
extern uint32_t __bss_start__, __bss_end__;
extern uint32_t __vectors;
extern void psram_main(void);

/*
 * This whole function executes from SRAM. Keep it self-contained: an
 * unannotated helper would jump back to PSRAM during sensitive startup.
 */
__attribute__((section(".sram_bootstrap"), noreturn, used))
void sram_bootstrap(void) {
    uint32_t *src = &__data_load_source__;
    for (uint32_t *dst = &__data_start__; dst < &__data_end__;)
        *dst++ = *src++;
    for (uint32_t *dst = &__bss_start__; dst < &__bss_end__;)
        *dst++ = 0;

    /* Cortex-M VTOR; the vector table remains at the PSRAM image base. */
    *(volatile uint32_t *)0xe000ed08u = (uint32_t)(uintptr_t)&__vectors;

    /*
     * Real clock/QMI transitions belong here, with every callee in SRAM.
     * This example preserves the loader's already-working configuration.
     */
    psram_main();
    for (;;) {}
}

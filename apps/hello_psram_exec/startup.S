.syntax unified
.cpu cortex-m33
.thumb

/* Copy the bootstrap to SRAM without touching clocks, QMI, or BOOTRAM. */
.section .text.psram_reset, "ax", %progbits
.global psram_reset
.type psram_reset, %function
.thumb_func
psram_reset:
    ldr r0, =__sram_boot_load_source__
    ldr r1, =__sram_boot_start__
    ldr r2, =__sram_boot_end__
1:
    cmp r1, r2
    bcs 2f
    ldr r3, [r0], #4
    str r3, [r1], #4
    b 1b
2:
    ldr r0, =sram_bootstrap
    bx r0
.size psram_reset, . - psram_reset

.global default_handler
.type default_handler, %function
.thumb_func
default_handler:
    b default_handler
.size default_handler, . - default_handler

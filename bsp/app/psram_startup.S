.syntax unified
.cpu cortex-m33
.thumb

.section .reset, "ax", %progbits
.global _entry_point
.global _reset_handler
.type _entry_point, %function
.type _reset_handler, %function
.thumb_func
_entry_point:
_reset_handler:
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
    ldr r0, =fw2_psram_bootstrap
    bx r0
.size _reset_handler, . - _reset_handler

.p2align 2
.global data_cpy_table
data_cpy_table:
    .word __data_load_source__
    .word __data_start__
    .word __data_end__
    .word 0

/*
 * Loader-launched PSRAM application layout. BOOTRAM is deliberately absent.
 * The DISPLAY loader has already initialized QMI CS1 and loaded this image.
 */
MEMORY
{
    PSRAM (rx)  : ORIGIN = 0x11000000, LENGTH = 8M
    SRAM  (rwx) : ORIGIN = 0x20000000, LENGTH = 448K
}

/*
 * Keep the ELF entry inside a mapped byte (picotool validates this). The
 * loader starts from the vector table, whose second word selects psram_reset.
 */
ENTRY(__vectors)

PHDRS
{
    psram PT_LOAD FLAGS(5); /* read + execute */
    boot  PT_LOAD FLAGS(5); /* copied to writable SRAM, then executed */
    data  PT_LOAD FLAGS(6); /* read + write */
}

SECTIONS
{
    .vectors ORIGIN(PSRAM) :
    {
        __vectors = .;
        LONG(__StackTop);
        LONG(psram_reset + 1);
        LONG(default_handler + 1); LONG(default_handler + 1);
        LONG(default_handler + 1); LONG(default_handler + 1);
        LONG(default_handler + 1); LONG(0);
        LONG(0); LONG(0); LONG(0);
        LONG(default_handler + 1); LONG(default_handler + 1);
        LONG(0); LONG(default_handler + 1); LONG(default_handler + 1);
        __vectors_end = .;
    } > PSRAM :psram

    .psram_text :
    {
        KEEP(*(.text.psram_reset))
        *(.text .text.*)
        *(.rodata .rodata.*)
        . = ALIGN(4);
    } > PSRAM :psram

    /* VMA is SRAM; load bytes remain in the contiguous PSRAM image. */
    .sram_bootstrap :
    {
        __sram_boot_start__ = .;
        KEEP(*(.sram_bootstrap .sram_bootstrap.*))
        . = ALIGN(4);
        __sram_boot_end__ = .;
    } > SRAM AT> PSRAM :boot
    __sram_boot_load_source__ = LOADADDR(.sram_bootstrap);

    .data :
    {
        __data_start__ = .;
        *(.data .data.*)
        . = ALIGN(4);
        __data_end__ = .;
    } > SRAM AT> PSRAM :data
    __data_load_source__ = LOADADDR(.data);

    .bss (NOLOAD) :
    {
        __bss_start__ = .;
        *(.bss .bss.* COMMON)
        . = ALIGN(4);
        __bss_end__ = .;
    } > SRAM :NONE

    .stack (NOLOAD) :
    {
        . = ALIGN(8);
        __StackBottom = .;
        . += 4096;
        __StackTop = .;
    } > SRAM :NONE

    /DISCARD/ : { *(.comment .note .ARM.attributes) }
}

ASSERT(ADDR(.vectors) == ORIGIN(PSRAM), "vector table must begin the PSRAM image")
ASSERT(__StackTop <= ORIGIN(SRAM) + LENGTH(SRAM), "SRAM overflow")
ASSERT(LOADADDR(.data) + SIZEOF(.data) <= ORIGIN(PSRAM) + LENGTH(PSRAM),
       "PSRAM image overflow")

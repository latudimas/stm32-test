/*
 * Linker script for STM32F103 (medium density, 64 KB flash / 20 KB RAM).
 * Memory map: RM0008 §3.3 "Memory map" (flash @ 0x08000000, SRAM @ 0x20000000).
 * Flash size 64 KB confirmed by the flash-size register @ 0x1FFFF7E0 = 0x0040.
 */

ENTRY(Reset_Handler)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 64K
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 20K
}

/* Full-descending stack starts at the top of RAM (PM0056 §2.1.2). */
_estack = ORIGIN(RAM) + LENGTH(RAM);

SECTIONS
{
    /* Vector table must sit at the start of flash: with BOOT0=0 the flash is
     * aliased to 0x00000000, where the core fetches initial SP and reset vector
     * (RM0008 §3.4 "Boot configuration"). */
    .isr_vector :
    {
        KEEP(*(.isr_vector))
    } > FLASH

    .text :
    {
        *(.text*)
    } > FLASH

    .rodata :
    {
        *(.rodata*)
        . = ALIGN(4);
    } > FLASH

    /* Initialised data: lives in RAM, but its initial values are stored in
     * flash right after .rodata. Reset_Handler copies _sidata -> _sdata.._edata. */
    .data :
    {
        _sdata = .;
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > RAM AT> FLASH

    _sidata = LOADADDR(.data);

    /* Zero-initialised data: Reset_Handler clears _sbss.._ebss. */
    .bss (NOLOAD) :
    {
        _sbss = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > RAM
}

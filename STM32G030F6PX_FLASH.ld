/*
 * STM32G030F6PX_FLASH.ld
 * Linker script for STM32G030F6P6
 *   Flash : 32 KB @ 0x08000000
 *   SRAM  : 8 KB  @ 0x20000000
 */
ENTRY(Reset_Handler)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 32K
    SRAM  (rwx) : ORIGIN = 0x20000000, LENGTH = 8K
}

_estack = ORIGIN(SRAM) + LENGTH(SRAM);

SECTIONS
{
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } >FLASH

    .text :
    {
        . = ALIGN(4);
        *(.text) *(.text*)
        *(.rodata) *(.rodata*)
        . = ALIGN(4);
        _etext = .;
    } >FLASH

    _sidata = LOADADDR(.data);
    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data) *(.data*)
        . = ALIGN(4);
        _edata = .;
    } >SRAM AT> FLASH

    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        __bss_start__ = _sbss;
        *(.bss) *(.bss*) *(COMMON)
        . = ALIGN(4);
        _ebss = .;
        __bss_end__ = _ebss;
    } >SRAM

    ._stack :
    {
        . = ALIGN(8);
        PROVIDE(end = .);
        . = . + 0x100;   /* 256 B heap (unused) */
        . = . + 0x600;   /* 1.5 KB stack        */
        . = ALIGN(8);
    } >SRAM

    /DISCARD/ : { libc.a(*) libm.a(*) libgcc.a(*) }
}

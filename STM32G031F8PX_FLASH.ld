/*
 * STM32G031F8PX_FLASH.ld
 * Linker script for STM32G031F8P6
 *   Flash    : 64 KB @ 0x08000000
 *     Pages 0-30 (62 KB) : application code
 *     Page  31   (2 KB)  : persistent config store (slave ID, etc.)
 *   SRAM     : 8 KB  @ 0x20000000
 */
ENTRY(Reset_Handler)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 62K  /* Pages 0-30: application code */
    CFGSTORE (r): ORIGIN = 0x0800F800, LENGTH = 2K   /* Page 31: persistent config (slave ID, etc.) */
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

    /* Config store page — reserved for runtime-writable persistent data.
     * No C objects are placed here by the compiler; the page is accessed
     * entirely via bsp_flash_* at the fixed address BSP_FLASH_CFG_PAGE_ADDR.
     * Declaring the region here prevents the linker from spilling code into
     * this address range and documents the reservation explicitly. */
    .cfgstore (NOLOAD) :
    {
        . = ALIGN(2048);
        KEEP(*(.cfgstore))
        . = ALIGN(2048);
    } >CFGSTORE
}

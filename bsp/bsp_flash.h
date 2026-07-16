/*
 * @file bsp_flash.h
 * @brief Flash self-programming BSP for STM32G031
 *
 * Created on: 15 July 2026
 *     Author: DST0x
 *
 * Description:
 *   Provides erase and write primitives for the internal Flash.
 *   Used to persist configuration data (e.g. Modbus slave ID) across resets.
 *
 *   Layout (STM32G031F8 — 64 KB, page size 2 KB, 32 pages):
 *     Pages 0-30 : application code (0x08000000 – 0x0800F7FF)
 *     Page 31    : config store     (0x0800F800 – 0x0800FFFF)  ← reserved
 *
 *   Config page layout (32 bytes used, written as 4 doublewords):
 *
 *     Doubleword 0 — Slave ID
 *       Offset 0x00  uint32_t  magic    BSP_FLASH_CFG_MAGIC (0xDEAD1234)
 *       Offset 0x04  uint32_t  payload  bits[7:0] = slave ID (1-247)
 *
 *     Doubleword 1 — Baudrate
 *       Offset 0x08  uint32_t  magic    BSP_FLASH_CFG_MAGIC
 *       Offset 0x0C  uint32_t  payload  baudrate in bps (e.g. 9600, 19200, …)
 *
 *     Doubleword 2 — Parity
 *       Offset 0x10  uint32_t  magic    BSP_FLASH_CFG_MAGIC
 *       Offset 0x14  uint32_t  payload  0=None, 1=Even, 2=Odd
 *
 *     Doubleword 3 — Stop bits
 *       Offset 0x18  uint32_t  magic    BSP_FLASH_CFG_MAGIC
 *       Offset 0x1C  uint32_t  payload  1=one stop bit, 2=two stop bits
 *
 *   Each entry has its own magic word so entries can be read/validated
 *   independently. The entire page is erased and all four doublewords are
 *   rewritten atomically whenever any config value changes.
 */

#ifndef BSP_FLASH_H
#define BSP_FLASH_H

#include <stdint.h>
#include "common/common_types.h"

/* -------------------------------------------------------------------------
 * Config page address (page 31, last 2 KB of 64 KB Flash)
 * ------------------------------------------------------------------------- */
#define BSP_FLASH_CFG_PAGE_ADDR  (0x0800F800UL)
#define BSP_FLASH_CFG_PAGE_NUM   (31U)           /* Zero-based page index    */
#define BSP_FLASH_PAGE_SIZE      (2048U)          /* STM32G0 page = 2 KB      */

/* Magic word written together with the payload to validate the stored data  */
#define BSP_FLASH_CFG_MAGIC      (0xDEAD1234UL)

/* Byte offsets within the config page for each doubleword entry.
 * Each entry: [magic @ offset+0][payload @ offset+4]                        */
#define BSP_FLASH_CFG_OFF_SLAVE_ID   (0x00U)   /* Slave ID entry             */
#define BSP_FLASH_CFG_OFF_BAUDRATE   (0x08U)   /* Baudrate entry             */
#define BSP_FLASH_CFG_OFF_PARITY     (0x10U)   /* Parity entry               */
#define BSP_FLASH_CFG_OFF_STOPBITS   (0x18U)   /* Stop bits entry            */

/* -------------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------------- */

/**
 * @brief Erase a single Flash page.
 *
 * Unlocks Flash, erases the requested page, then re-locks.
 * Caller must ensure interrupts that access Flash are disabled or
 * will not fire during the operation (safe here — no XIP from RAM).
 *
 * @param page  Zero-based page number (0–31 for STM32G031F8).
 * @return STATUS_OK on success, STATUS_ERR_GENERIC on Flash error/timeout.
 */
status_e bsp_flash_erase_page(uint8_t page);

/**
 * @brief Write one 64-bit doubleword to Flash.
 *
 * The target address must be 8-byte aligned and already erased (all 0xFF).
 * Unlocks Flash, writes, then re-locks.
 *
 * @param addr   Destination address (must be 8-byte aligned).
 * @param word_lo Lower 32 bits.
 * @param word_hi Upper 32 bits.
 * @return STATUS_OK on success, STATUS_ERR_GENERIC on error.
 */
status_e bsp_flash_write_dword(uint32_t addr, uint32_t word_lo, uint32_t word_hi);

/**
 * @brief Read the 32-bit word at a given Flash address.
 *
 * Flash is memory-mapped; this is a plain pointer dereference.
 *
 * @param addr Source address.
 * @return Value at address.
 */
static inline uint32_t bsp_flash_read_word(uint32_t addr)
{
    return *((volatile uint32_t *)addr);
}

/**
 * @brief Write all four config entries to the config page atomically.
 *
 * Erases the page once then writes slave_id, baudrate, parity, and stop bits
 * as four consecutive doublewords. Always call this function instead of
 * calling bsp_flash_write_dword directly for config data, so all values
 * stay consistent on the page.
 *
 * @param slave_id  Modbus slave ID (1–247)
 * @param baudrate  UART baudrate in bps (e.g. 9600, 19200, 38400, 115200)
 * @param parity    0=None  1=Even  2=Odd
 * @param stopbits  1=one stop bit  2=two stop bits
 * @return STATUS_OK if all writes verified, STATUS_ERR_GENERIC on failure.
 */
status_e bsp_flash_write_config(uint8_t  slave_id,
                                uint32_t baudrate,
                                uint8_t  parity,
                                uint8_t  stopbits);

#endif /* BSP_FLASH_H */

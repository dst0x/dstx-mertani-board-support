/*
 * @file bsp_flash.c
 * @brief Flash self-programming BSP for STM32G031
 *
 * Created on: 15 July 2026
 *     Author: DST0x
 */

#include "stm32g0xx.h"
#include "bsp/bsp_flash.h"

#define FLASH_KEY1          (0x45670123UL)
#define FLASH_KEY2          (0xCDEF89ABUL)
#define FLASH_TIMEOUT       (100000UL)

/* STM32G0 error bits (RDERR/OPTVERR not present on this family) */
#define FLASH_SR_ERR_MASK   (FLASH_SR_WRPERR  | \
                             FLASH_SR_PGAERR  | \
                             FLASH_SR_SIZERR  | \
                             FLASH_SR_PGSERR  | \
                             FLASH_SR_MISERR  | \
                             FLASH_SR_FASTERR)

static void flash_unlock(void)
{
    if ((FLASH->CR & FLASH_CR_LOCK) != 0UL) {
        FLASH->KEYR = FLASH_KEY1;
        FLASH->KEYR = FLASH_KEY2;
    }
}

static void flash_lock(void)
{
    SET_BIT(FLASH->CR, FLASH_CR_LOCK);
}

static status_e flash_wait_ready(void)
{
    uint32_t count = 0U;
    while ((FLASH->SR & FLASH_SR_BSY1) != 0UL) {
        if (++count >= FLASH_TIMEOUT) {
            return STATUS_ERR_TIMEOUT;
        }
    }
    return STATUS_OK;
}

static status_e flash_check_and_clear_errors(void)
{
    uint32_t errors = FLASH->SR & FLASH_SR_ERR_MASK;
    if (errors != 0UL) {
        FLASH->SR = errors;
        return STATUS_ERR_GENERIC;
    }
    return STATUS_OK;
}

status_e bsp_flash_erase_page(uint8_t page)
{
    status_e ret;

    ret = flash_wait_ready();
    if (ret != STATUS_OK) { return ret; }

    flash_unlock();
    FLASH->SR = FLASH_SR_ERR_MASK;

    MODIFY_REG(FLASH->CR,
               FLASH_CR_PNB_Msk | FLASH_CR_PER | FLASH_CR_PG,
               ((uint32_t)page << FLASH_CR_PNB_Pos) | FLASH_CR_PER);
    SET_BIT(FLASH->CR, FLASH_CR_STRT);

    ret = flash_wait_ready();
    CLR_BIT(FLASH->CR, FLASH_CR_PER | FLASH_CR_PNB_Msk);

    if (ret == STATUS_OK) {
        ret = flash_check_and_clear_errors();
    }

    flash_lock();
    return ret;
}

status_e bsp_flash_write_dword(uint32_t addr, uint32_t word_lo, uint32_t word_hi)
{
    status_e ret;
    volatile uint32_t *p = (volatile uint32_t *)addr;

    if ((addr & 0x07UL) != 0UL) {
        return STATUS_ERR_PARAM;
    }

    ret = flash_wait_ready();
    if (ret != STATUS_OK) { return ret; }

    flash_unlock();
    FLASH->SR = FLASH_SR_ERR_MASK;

    SET_BIT(FLASH->CR, FLASH_CR_PG);
    p[0U] = word_lo;
    p[1U] = word_hi;

    ret = flash_wait_ready();
    CLR_BIT(FLASH->CR, FLASH_CR_PG);

    if (ret == STATUS_OK) {
        ret = flash_check_and_clear_errors();
    }
    if (ret == STATUS_OK) {
        if ((p[0U] != word_lo) || (p[1U] != word_hi)) {
            ret = STATUS_ERR_GENERIC;
        }
    }

    flash_lock();
    return ret;
}

status_e bsp_flash_write_config(uint8_t  slave_id,
                                uint32_t baudrate,
                                uint8_t  parity,
                                uint8_t  stopbits)
{
    status_e ret;

    ret = bsp_flash_erase_page(BSP_FLASH_CFG_PAGE_NUM);
    if (ret != STATUS_OK) { return ret; }

    ret = bsp_flash_write_dword(BSP_FLASH_CFG_PAGE_ADDR + BSP_FLASH_CFG_OFF_SLAVE_ID,
                                BSP_FLASH_CFG_MAGIC, (uint32_t)slave_id);
    if (ret != STATUS_OK) { return ret; }

    ret = bsp_flash_write_dword(BSP_FLASH_CFG_PAGE_ADDR + BSP_FLASH_CFG_OFF_BAUDRATE,
                                BSP_FLASH_CFG_MAGIC, baudrate);
    if (ret != STATUS_OK) { return ret; }

    ret = bsp_flash_write_dword(BSP_FLASH_CFG_PAGE_ADDR + BSP_FLASH_CFG_OFF_PARITY,
                                BSP_FLASH_CFG_MAGIC, (uint32_t)parity);
    if (ret != STATUS_OK) { return ret; }

    ret = bsp_flash_write_dword(BSP_FLASH_CFG_PAGE_ADDR + BSP_FLASH_CFG_OFF_STOPBITS,
                                BSP_FLASH_CFG_MAGIC, (uint32_t)stopbits);
    return ret;
}

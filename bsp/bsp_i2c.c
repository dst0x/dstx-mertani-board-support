#include "bsp/bsp_i2c.h"
#include "bsp/bsp_systick.h"
#include "bsp/bsp_iwdg.h"
#include "stm32g0xx.h"

#define I2C2_TIMINGR     (0x9032191FUL)
#define I2C_TIMEOUT_MS   (50U)  /* 50ms timeout for I2C operations */

static status_e wait_flag_set(volatile const uint32_t *reg, uint32_t mask, uint32_t timeout_ms){
    uint32_t start_tick = bsp_systick_get_tick();
    while(!bsp_systick_elapsed(start_tick, timeout_ms)){
        if ((*reg & mask) != 0UL) {return STATUS_OK;}
        
        /* Refresh watchdog periodically during wait */
        if ((bsp_systick_get_tick() - start_tick) % 10U == 0U) {
            bsp_iwdg_refresh();
        }
    }
    return STATUS_ERR_TIMEOUT;
}

static status_e check_nack(void){
    if ((I2C2->ISR & I2C_ISR_NACKF) != 0UL) {
        SET_BIT(I2C2->ICR, I2C_ICR_NACKCF);
        SET_BIT(I2C2->CR2, I2C_CR2_STOP);
        return STATUS_ERR_I2C;
    }
    return STATUS_OK;
}

void bsp_i2c_init(void){
    SET_BIT(RCC->APBENR1, RCC_APBENR1_I2C2EN);
    (void)RCC->APBENR1;
    SET_BIT(RCC->APBRSTR1, RCC_APBRSTR1_I2C2RST);
    CLR_BIT(RCC->APBRSTR1, RCC_APBRSTR1_I2C2RST);
    CLR_BIT(I2C2->CR1, I2C_CR1_PE);
    I2C2->TIMINGR = I2C2_TIMINGR;
    I2C2->CR1     = 0U;
    SET_BIT(I2C2->CR1, I2C_CR1_PE);
}

void bsp_i2c_bus_recover(void){
    uint8_t i;
    GPIOA->MODER &= ~(3UL << (11U * 2U));
    GPIOA->MODER |=  (1UL << (11U * 2U));
    for (i = 0U; i < 9U; i++){
        GPIOA->BSRR = (1UL << 11U);
        bsp_systick_delay_ms(1U);
        GPIOA->BSRR = (1UL << (11U + 16U));
        bsp_systick_delay_ms(1U);
        
        /* Refresh IWDG during bus recovery */
        bsp_iwdg_refresh();
    }
    GPIOA->MODER &= ~(3UL << (11U * 2U));
    GPIOA->MODER |=  (2UL << (11U * 2U));
}

status_e bsp_i2c_write_cmd(uint8_t addr, uint16_t cmd){
    status_e status;
    uint8_t  cmd_hi = (uint8_t)((cmd >> 8U) & 0xFFU);
    uint8_t  cmd_lo = (uint8_t)(cmd & 0xFFU);

    I2C2->ICR = 0xFFFFFFFFUL;

    if ((I2C2->ISR & I2C_ISR_BUSY) != 0UL){
        bsp_i2c_bus_recover();
        bsp_systick_delay_ms(10U);
    }

    MODIFY_REG(I2C2->CR2,
               I2C_CR2_SADD | I2C_CR2_NBYTES | I2C_CR2_RD_WRN | I2C_CR2_AUTOEND,
               ((uint32_t)addr << 1U) | (2UL << I2C_CR2_NBYTES_Pos) | I2C_CR2_AUTOEND);
    SET_BIT(I2C2->CR2, I2C_CR2_START);

    status = wait_flag_set(&I2C2->ISR, I2C_ISR_TXIS, I2C_TIMEOUT_MS);
    if (status != STATUS_OK) { return STATUS_ERR_I2C; }
    if (check_nack() != STATUS_OK) { return STATUS_ERR_I2C; }
    I2C2->TXDR = (uint32_t)cmd_hi;

    status = wait_flag_set(&I2C2->ISR, I2C_ISR_TXIS, I2C_TIMEOUT_MS);
    if (status != STATUS_OK) { return STATUS_ERR_I2C; }
    if (check_nack() != STATUS_OK) { return STATUS_ERR_I2C; }
    I2C2->TXDR = (uint32_t)cmd_lo;

    status = wait_flag_set(&I2C2->ISR, I2C_ISR_STOPF, I2C_TIMEOUT_MS);
    SET_BIT(I2C2->ICR, I2C_ICR_STOPCF);
    return status;
}

status_e bsp_i2c_write_data(uint8_t addr, uint16_t cmd, uint8_t *buf, uint8_t len){
    status_e status;
    uint8_t  cmd_hi = (uint8_t)((cmd >> 8U) & 0xFFU);
    uint8_t  cmd_lo = (uint8_t)(cmd & 0xFFU);
    uint8_t  i;

    if ((buf == NULL) || (len == 0U)) { return STATUS_ERR_PARAM; }

    I2C2->ICR = 0xFFFFFFFFUL;

    MODIFY_REG(I2C2->CR2,
               I2C_CR2_SADD | I2C_CR2_NBYTES | I2C_CR2_RD_WRN | I2C_CR2_AUTOEND,
               ((uint32_t)addr << 1U) | (2UL << I2C_CR2_NBYTES_Pos) | I2C_CR2_AUTOEND);
    SET_BIT(I2C2->CR2, I2C_CR2_START);

    status = wait_flag_set(&I2C2->ISR, I2C_ISR_TXIS, I2C_TIMEOUT_MS);
    if (status != STATUS_OK) { return STATUS_ERR_I2C; }
    if (check_nack() != STATUS_OK) { return STATUS_ERR_I2C; }
    I2C2->TXDR = (uint32_t)cmd_hi;

    status = wait_flag_set(&I2C2->ISR, I2C_ISR_TXIS, I2C_TIMEOUT_MS);
    if (status != STATUS_OK) { return STATUS_ERR_I2C; }
    if (check_nack() != STATUS_OK) { return STATUS_ERR_I2C; }
    I2C2->TXDR = (uint32_t)cmd_lo;

    status = wait_flag_set(&I2C2->ISR, I2C_ISR_STOPF, I2C_TIMEOUT_MS);
    SET_BIT(I2C2->ICR, I2C_ICR_STOPCF);
    if (status != STATUS_OK) { return STATUS_ERR_I2C; }

    bsp_systick_delay_ms(20U);

    I2C2->ICR = 0xFFFFFFFFUL;
    MODIFY_REG(I2C2->CR2,
               I2C_CR2_SADD | I2C_CR2_NBYTES | I2C_CR2_RD_WRN | I2C_CR2_AUTOEND,
               ((uint32_t)addr << 1U) | ((uint32_t)len << I2C_CR2_NBYTES_Pos) |
               I2C_CR2_RD_WRN | I2C_CR2_AUTOEND);
    SET_BIT(I2C2->CR2, I2C_CR2_START);

    for (i = 0U; i < len; i++)
    {
        status = wait_flag_set(&I2C2->ISR, I2C_ISR_RXNE, I2C_TIMEOUT_MS);
        if (status != STATUS_OK) { return STATUS_ERR_I2C; }
        buf[i] = (uint8_t)(I2C2->RXDR & 0xFFUL);
    }

    status = wait_flag_set(&I2C2->ISR, I2C_ISR_STOPF, I2C_TIMEOUT_MS);
    SET_BIT(I2C2->ICR, I2C_ICR_STOPCF);
    return status;
}

status_e bsp_i2c_read_data(uint8_t addr, uint8_t *buf, uint8_t len){
    status_e status;
    uint8_t  i;

    if ((buf == NULL) || (len == 0U)) { return STATUS_ERR_PARAM; }

    I2C2->ICR = 0xFFFFFFFFUL;

    MODIFY_REG(I2C2->CR2,
               I2C_CR2_SADD | I2C_CR2_NBYTES | I2C_CR2_RD_WRN | I2C_CR2_AUTOEND,
               ((uint32_t)addr << 1U) | ((uint32_t)len << I2C_CR2_NBYTES_Pos) |
               I2C_CR2_RD_WRN | I2C_CR2_AUTOEND);
    SET_BIT(I2C2->CR2, I2C_CR2_START);

    for (volatile uint8_t d = 0; d < 100U; d++){ }

    for (i = 0U; i < len; i++)
    {
        status = wait_flag_set(&I2C2->ISR, I2C_ISR_RXNE, I2C_TIMEOUT_MS);
        if (status != STATUS_OK) { return STATUS_ERR_I2C; }
        buf[i] = (uint8_t)(I2C2->RXDR & 0xFFUL);
    }

    status = wait_flag_set(&I2C2->ISR, I2C_ISR_STOPF, I2C_TIMEOUT_MS);
    SET_BIT(I2C2->ICR, I2C_ICR_STOPCF);
    return status;
}

status_e bsp_i2c_scan(uint8_t addr){
    uint32_t start_tick;
    
    I2C2->ICR = 0xFFFFFFFFUL;
    MODIFY_REG(I2C2->CR2,
               I2C_CR2_SADD | I2C_CR2_NBYTES | I2C_CR2_RD_WRN | I2C_CR2_AUTOEND,
               ((uint32_t)addr << 1U) | I2C_CR2_AUTOEND);
    SET_BIT(I2C2->CR2, I2C_CR2_START);

    start_tick = bsp_systick_get_tick();
    while (!bsp_systick_elapsed(start_tick, I2C_TIMEOUT_MS))
    {
        if ((I2C2->ISR & I2C_ISR_NACKF) != 0UL)
        {
            SET_BIT(I2C2->ICR, I2C_ICR_NACKCF | I2C_ICR_STOPCF);
            return STATUS_ERR_I2C;
        }
        if ((I2C2->ISR & I2C_ISR_STOPF) != 0UL)
        {
            SET_BIT(I2C2->ICR, I2C_ICR_STOPCF);
            return STATUS_OK;
        }
    }
    return STATUS_ERR_TIMEOUT;
}
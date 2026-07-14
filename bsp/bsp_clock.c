#include "bsp/bsp_clock.h"
#include "stm32g0xx.h"

#define CLOCK_TIMEOUT (100000UL)
#define BSP_PLLM    (0U)
#define BSP_PLLN    (8U)
#define BSP_PLLR    (2U)

static status_e wait_flag (volatile uint32_t *reg, uint32_t mask, uint32_t timeout){
    uint32_t count = 0U;
    while(count < timeout){
        if((*reg & mask) == mask){
            return STATUS_OK;
        }
        count++;
    }
    return STATUS_ERR_TIMEOUT;
}

status_e bsp_clock_init(void){
    status_e status;

    SET_BIT(RCC->CR, RCC_CR_HSION);
    status = wait_flag(&RCC->CR, RCC_CR_HSIRDY, CLOCK_TIMEOUT);
    if (status != STATUS_OK) { return STATUS_ERR_TIMEOUT; }

    MODIFY_REG(FLASH->ACR, FLASH_ACR_LATENCY, FLASH_ACR_LATENCY_2);
    if ((FLASH->ACR & FLASH_ACR_LATENCY) != FLASH_ACR_LATENCY_2) { return STATUS_ERR_GENERIC; }

    SET_BIT(FLASH->ACR, FLASH_ACR_PRFTEN);

    CLR_BIT(RCC->CR, RCC_CR_PLLON);
    while ((RCC->CR & RCC_CR_PLLRDY) != 0UL) { }

    RCC->PLLCFGR = (RCC_PLLCFGR_PLLSRC_HSI) |
                   (BSP_PLLM << RCC_PLLCFGR_PLLM_Pos) |
                   (BSP_PLLN << RCC_PLLCFGR_PLLN_Pos) |
                   (BSP_PLLR << RCC_PLLCFGR_PLLR_Pos) |
                   RCC_PLLCFGR_PLLREN;

    SET_BIT(RCC->CR, RCC_CR_PLLON);
    status = wait_flag(&RCC->CR, RCC_CR_PLLRDY, CLOCK_TIMEOUT);
    if (status != STATUS_OK) { return STATUS_ERR_TIMEOUT; }

    MODIFY_REG(RCC->CFGR, RCC_CFGR_SW, RCC_CFGR_SW_1);
    status = wait_flag(&RCC->CFGR, RCC_CFGR_SWS_1, CLOCK_TIMEOUT);
    if (status != STATUS_OK) { return STATUS_ERR_TIMEOUT; }

    return STATUS_OK;
}
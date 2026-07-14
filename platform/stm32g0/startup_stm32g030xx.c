/**
 * @file    startup_stm32g030xx.c
 * @brief   Cortex-M0+ vector table and reset handler for STM32G030.
 * @note    MISRA C:2012 compliant.
 */
#include <stdint.h>

extern int  main(void);
extern uint32_t _sidata, _sdata, _edata, _sbss, _ebss, _estack;

void Default_Handler(void);
void Reset_Handler(void);

#define WEAK_ALIAS(x) __attribute__((weak, alias(#x)))

void NMI_Handler(void)                          WEAK_ALIAS(Default_Handler);
void HardFault_Handler(void)                    WEAK_ALIAS(Default_Handler);
void SVC_Handler(void)                          WEAK_ALIAS(Default_Handler);
void PendSV_Handler(void)                       WEAK_ALIAS(Default_Handler);
void SysTick_Handler(void)                      WEAK_ALIAS(Default_Handler);
void WWDG_IRQHandler(void)                      WEAK_ALIAS(Default_Handler);
void PVD_IRQHandler(void)                       WEAK_ALIAS(Default_Handler);
void RTC_TAMP_IRQHandler(void)                  WEAK_ALIAS(Default_Handler);
void FLASH_IRQHandler(void)                     WEAK_ALIAS(Default_Handler);
void RCC_IRQHandler(void)                       WEAK_ALIAS(Default_Handler);
void EXTI0_1_IRQHandler(void)                   WEAK_ALIAS(Default_Handler);
void EXTI2_3_IRQHandler(void)                   WEAK_ALIAS(Default_Handler);
void EXTI4_15_IRQHandler(void)                  WEAK_ALIAS(Default_Handler);
void DMA1_Channel1_IRQHandler(void)             WEAK_ALIAS(Default_Handler);
void DMA1_Channel2_3_IRQHandler(void)           WEAK_ALIAS(Default_Handler);
void DMA1_Ch4_7_DMAMUX1_OVR_IRQHandler(void)   WEAK_ALIAS(Default_Handler);
void ADC1_IRQHandler(void)                      WEAK_ALIAS(Default_Handler);
void TIM1_BRK_UP_TRG_COM_IRQHandler(void)       WEAK_ALIAS(Default_Handler);
void TIM1_CC_IRQHandler(void)                   WEAK_ALIAS(Default_Handler);
void TIM3_IRQHandler(void)                      WEAK_ALIAS(Default_Handler);
void TIM14_IRQHandler(void)                     WEAK_ALIAS(Default_Handler);
void TIM16_IRQHandler(void)                     WEAK_ALIAS(Default_Handler);
void TIM17_IRQHandler(void)                     WEAK_ALIAS(Default_Handler);
void I2C1_IRQHandler(void)                      WEAK_ALIAS(Default_Handler);
void I2C2_IRQHandler(void)                      WEAK_ALIAS(Default_Handler);
void SPI1_IRQHandler(void)                      WEAK_ALIAS(Default_Handler);
void SPI2_IRQHandler(void)                      WEAK_ALIAS(Default_Handler);
void USART1_IRQHandler(void)                    WEAK_ALIAS(Default_Handler);
void USART2_IRQHandler(void)                    WEAK_ALIAS(Default_Handler);

typedef void (*VectorEntry_t)(void);

__attribute__((section(".isr_vector"), used))
static const VectorEntry_t s_VectorTable[] =
{
    (VectorEntry_t)&_estack,     /* Initial stack pointer */
    Reset_Handler,               /* Reset                 */
    NMI_Handler,
    HardFault_Handler,
    0, 0, 0, 0, 0, 0, 0,        /* Reserved              */
    SVC_Handler,
    0, 0,                        /* Reserved              */
    PendSV_Handler,
    SysTick_Handler,
    /* External interrupts */
    WWDG_IRQHandler,                     /* IRQ0  */
    PVD_IRQHandler,                      /* IRQ1  */
    RTC_TAMP_IRQHandler,                 /* IRQ2  */
    FLASH_IRQHandler,                    /* IRQ3  */
    RCC_IRQHandler,                      /* IRQ4  */
    EXTI0_1_IRQHandler,                  /* IRQ5  */
    EXTI2_3_IRQHandler,                  /* IRQ6  */
    EXTI4_15_IRQHandler,                 /* IRQ7  */
    0,                                   /* IRQ8  Reserved */
    DMA1_Channel1_IRQHandler,            /* IRQ9  */
    DMA1_Channel2_3_IRQHandler,          /* IRQ10 */
    DMA1_Ch4_7_DMAMUX1_OVR_IRQHandler,  /* IRQ11 */
    ADC1_IRQHandler,                     /* IRQ12 */
    TIM1_BRK_UP_TRG_COM_IRQHandler,      /* IRQ13 */
    TIM1_CC_IRQHandler,                  /* IRQ14 */
    0,                                   /* IRQ15 Reserved */
    TIM3_IRQHandler,                     /* IRQ16 */
    0, 0,                                /* IRQ17-18 Reserved */
    TIM14_IRQHandler,                    /* IRQ19 */
    0,                                   /* IRQ20 Reserved */
    TIM16_IRQHandler,                    /* IRQ21 */
    TIM17_IRQHandler,                    /* IRQ22 */
    I2C1_IRQHandler,                     /* IRQ23 */
    I2C2_IRQHandler,                     /* IRQ24 */
    SPI1_IRQHandler,                     /* IRQ25 */
    SPI2_IRQHandler,                     /* IRQ26 */
    USART1_IRQHandler,                   /* IRQ27 */
    USART2_IRQHandler,                   /* IRQ28 */
    0                                    /* IRQ29 Reserved */
};

void Reset_Handler(void)
{
    uint32_t *pSrc = &_sidata;
    uint32_t *pDst = &_sdata;
    while (pDst < &_edata) { *pDst++ = *pSrc++; }
    pDst = &_sbss;
    while (pDst < &_ebss)  { *pDst++ = 0UL; }
    (void)main();
    while (1) { }
}

void Default_Handler(void) { while (1) { } }

#include "bsp/bsp_systick.h"
#include "stm32g0xx.h"

static volatile uint32_t tick_count = 0U;

void bsp_systick_init(void){
    (void)SysTick_Config(SYSCLK_HZ / 1000UL);
    NVIC_SetPriority(SysTick_IRQn, 3U);
}

void SysTick_Handler(void){
    tick_count++;
}

uint32_t bsp_systick_get_tick(void){
    return tick_count;
}

void bsp_systick_delay_ms(uint32_t ms){
    uint32_t start_tick = tick_count;
    while((tick_count - start_tick) < ms) { }
}

bool bsp_systick_elapsed(uint32_t start_tick, uint32_t period_ms){
    return ((tick_count - start_tick) >= period_ms);
}
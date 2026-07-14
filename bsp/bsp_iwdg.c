#include "bsp/bsp_iwdg.h"
#include "stm32g0xx.h"

#define IWDG_KEY_ENABLE     (0xCCCCU)
#define IWDG_KEY_WRITE      (0x5555U)
#define IWDG_KEY_REFRESH    (0xAAAAU)
#define IWDG_PR_DIV64       (4U)
#define IWDG_RELOAD         (1000U)

void bsp_iwdg_init(void){
    IWDG->KR  = IWDG_KEY_ENABLE;
    IWDG->KR  = IWDG_KEY_WRITE;
    IWDG->PR  = IWDG_PR_DIV64;
    IWDG->RLR = IWDG_RELOAD;
    while ((IWDG->SR & (IWDG_SR_PVU | IWDG_SR_RVU)) != 0UL) { }
    IWDG->KR  = IWDG_KEY_REFRESH;
}

void bsp_iwdg_refresh(void){
    IWDG->KR = IWDG_KEY_REFRESH;
}
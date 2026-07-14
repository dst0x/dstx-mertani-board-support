#ifndef BSP_SYSTICK_H
#define BSP_SYSTICK_H

#include "common/common_types.h"

void bsp_systick_init(void);
void bsp_systick_delay_ms(uint32_t ms);
uint32_t bsp_systick_get_tick(void);
bool bsp_systick_elapsed(uint32_t start_tick, uint32_t period_ms);

#endif /* BSP_SYSTICK_H */
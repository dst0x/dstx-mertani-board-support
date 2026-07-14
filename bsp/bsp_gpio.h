#ifndef BSP_GPIO_H
#define BSP_GPIO_H

#include "common/common_types.h"

typedef enum comm_mode_tag {
    COMM_MODE_RS485 = 0x00U,
    COMM_MODE_RS232 = 0x01U,
    COMM_MODE_SDI12 = 0x02U,
    COMM_MODE_TTL   = 0x03U
} comm_mode_e;

void bsp_gpio_init(void);
void bsp_gpio_led_on(void);
void bsp_gpio_led_off(void);
void bsp_gpio_led_toggle(void);
void bsp_gpio_rs485_drive_mode(void);
void bsp_gpio_rs485_logging_mode(void);
void bsp_gpio_set_comm_mode(comm_mode_e mode);

#endif /* BSP_GPIO_H */
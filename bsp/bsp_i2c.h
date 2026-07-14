#ifndef BSP_I2C_H
#define BSP_I2C_H

#include "common/common_types.h"

void bsp_i2c_init(void);
void bsp_i2c_bus_recover(void);

status_e bsp_i2c_write_cmd(uint8_t addr, uint16_t cmd);
status_e bsp_i2c_write_data(uint8_t addr, uint16_t cmd, uint8_t *buf, uint8_t len);
status_e bsp_i2c_read_data(uint8_t addr, uint8_t *buf, uint8_t len);
status_e bsp_i2c_scan(uint8_t addr);

#endif /* BSP_I2C_H */
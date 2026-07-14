#ifndef BSP_UART_H
#define BSP_UART_H

#include "common/common_types.h"

#define DEBUG_BUFFER_SIZE   (256U)
#define MODBUS_BUFFER_SIZE  (64U)
#define RS485_BUFFER_SIZE   (16U)

#define USART1_DEBUG_MODE
/* #define USART1_MODBUS_MODE */

#if defined(USART1_SENSOR_MODE) && defined(USART1_DEBUG_MODE)
    #error "Cannot enable both USART1_SENSOR_MODE and USART1_DEBUG_MODE simultaneously!"
#endif

#if !defined(USART1_SENSOR_MODE) && !defined(USART1_DEBUG_MODE)
    #error "Must define either USART1_SENSOR_MODE or USART1_DEBUG_MODE!"
#endif

/* USART1 Debug Mode */
#ifdef USART1_DEBUG_MODE
    void bsp_debug_init(void);
    void bsp_debug_write_str(const char *str);
    void bsp_debug_write_int(const char *prefix, int32_t value, const char *suffix);
    void bsp_debug_write_fixed(const char *prefix, int32_t raw,int32_t scale, uint8_t decimal_places, const char *suffix);
    void bsp_debug_write_hex(const char *prefix, const uint8_t *buf, uint8_t len, const char *suffix);
    uint16_t bsp_debug_write(const uint8_t *data, uint16_t len);
#endif /* USART1 Debug Mode */

/* USART1 MODBUS Mode */
#ifdef USART1_MODBUS_MODE
    void bsp_modbus_init(void);
    void bsp_modbus_rx_flush(void);
    uint16_t bsp_modbus_write(const uint8_t *data, uint16_t len);
    uint16_t bsp_modbus_rx_count(void);
    uint8_t bsp_modbus_rx_get(void);
#endif /* USART1 MODBUS Mode */

/* USART2 RS485 Mode */
void bsp_usart1_irq_handler(void);
void bsp_rs485_init(void);
void bsp_rs485_send(const uint8_t *data, uint16_t len);
void bsp_rs485_rx_flush(void);
void bsp_rs485_usart2_irq_handler(void);
uint16_t bsp_rs485_rx_count(void);
uint8_t bsp_rs485_rx_get(void);

#endif /* BSP_UART_H */
#ifndef BSP_UART_H
#define BSP_UART_H

#include "common/common_types.h"
#include "middleware/uart_manager.h" 

#define DEBUG_BUFFER_SIZE   (256U)
#define RS485_BUFFER_SIZE   (16U)

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
    struct co_ctx_tag;
    
    void bsp_modbus_init(void);
    void bsp_modbus_rx_flush(void);
    uint16_t bsp_modbus_write(const uint8_t *data, uint16_t len);
    uint16_t bsp_modbus_rx_count(void);
    uint8_t bsp_modbus_rx_get(void);
    
    void bsp_modbus_set_co_sensor_ptr(struct co_ctx_tag *ptr);
#endif /* USART1 MODBUS Mode */

/* USART1 Sensor Mode (PMSX003, etc.) */
#ifdef USART1_SENSOR_MODE
    struct pmsx003_ctx_tag;
    void bsp_sensor_init(void);
    void bsp_sensor_set_pmsx003_ptr(struct pmsx003_ctx_tag *ptr);
#endif /* USART1 Sensor Mode */

/* USART2 RS485 Mode */
void bsp_usart1_irq_handler(void);
void bsp_rs485_init(void);

/**
 * @brief Reconfigure USART2 baudrate, parity, and stop bits at runtime.
 *
 * Disables USART2, applies the new configuration, then re-enables it.
 * Safe to call from the main loop (not from an ISR).
 * The USART2 RX buffer is flushed before returning.
 *
 * @param baudrate  Baud rate in bps. Supported: 1200, 2400, 4800, 9600,
 *                  19200, 38400, 57600, 115200. Other values are rejected
 *                  and the function returns STATUS_ERR_PARAM.
 * @param parity    0 = None, 1 = Even, 2 = Odd.
 * @param stopbits  1 = 1 stop bit, 2 = 2 stop bits.
 * @return STATUS_OK on success, STATUS_ERR_PARAM if any argument is invalid.
 */
status_e bsp_rs485_reinit(uint32_t baudrate, uint8_t parity, uint8_t stopbits);

void bsp_rs485_send(const uint8_t *data, uint16_t len);
void bsp_rs485_rx_flush(void);
void bsp_rs485_usart2_irq_handler(void);
uint16_t bsp_rs485_rx_count(void);
uint8_t bsp_rs485_rx_get(void);

#endif /* BSP_UART_H */
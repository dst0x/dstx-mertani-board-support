/*
 * @file uart_manager.c
 * 
 * Created on: 15 July 2026
 *     Author: DST0x
 */

#include "uart_manager.h"
#include "bsp/bsp_uart.h"
#include "bsp/bsp_gpio.h"

static const char* get_mode_string(comm_path_mode_e mode) {
    switch (mode) {
        case COMM_PATH_MODE_RS485: return "RS485";
        case COMM_PATH_MODE_RS232: return "RS232";
        case COMM_PATH_MODE_TTL:   return "TTL";
        case COMM_PATH_MODE_SDI12: return "SDI-12";
        default:                   return "DISABLED";
    }
}

static uint32_t get_baudrate_for_mode(comm_path_mode_e mode) {
    switch (mode) {
        case COMM_PATH_MODE_RS485: return COMM_RS485_BAUDRATE;
        case COMM_PATH_MODE_RS232: return COMM_RS232_BAUDRATE;
        case COMM_PATH_MODE_TTL:   return COMM_TTL_BAUDRATE;
        case COMM_PATH_MODE_SDI12: return COMM_SDI12_BAUDRATE;
        default:                   return 9600U;
    }
}

status_e uart_manager_init(uart_manager_ctx_s *ctx) {
    if (ctx == NULL) {
        return STATUS_ERR_GENERIC;
    }

    /* Initialize context */
    ctx->usart1_mode = USART1_MODE;
    ctx->usart2_mode = USART2_MODE;
    ctx->usart1_baudrate = get_baudrate_for_mode(ctx->usart1_mode);
    ctx->usart2_baudrate = get_baudrate_for_mode(ctx->usart2_mode);
    ctx->usart1_tx_count = 0;
    ctx->usart1_rx_count = 0;
    ctx->usart2_tx_count = 0;
    ctx->usart2_rx_count = 0;
    ctx->usart1_errors = 0;
    ctx->usart2_errors = 0;

    /* Configure USART1 based on selected mode */
    switch (ctx->usart1_mode) {
        case COMM_PATH_MODE_TTL:
#ifdef USART1_DEBUG_MODE
            bsp_gpio_set_comm_mode(COMM_MODE_TTL);
            bsp_debug_init();
            ctx->usart1_initialized = true;
            
            /* Print initialization message */
            bsp_debug_write_str("\r\n=================================\r\n");
            bsp_debug_write_str("MERTANI BSP V1.0\r\n");
            bsp_debug_write_str("UART MANAGER - TTL DEBUG MODE\r\n");
            bsp_debug_write_str("=================================\r\n");
#endif
            break;

        case COMM_PATH_MODE_RS485:
        case COMM_PATH_MODE_RS232:
#ifdef USART1_MODBUS_MODE
            bsp_gpio_set_comm_mode((ctx->usart1_mode == COMM_PATH_MODE_RS485) ? 
                                   COMM_MODE_RS485 : COMM_MODE_RS232);
            bsp_modbus_init();
            ctx->usart1_initialized = true;
#endif
            break;

        case COMM_PATH_MODE_SDI12:
#ifdef USART1_SENSOR_MODE
            bsp_sensor_init();
            ctx->usart1_initialized = true;
#ifdef USART1_DEBUG_MODE
            bsp_debug_write_str("[UART_MGR] USART1 SENSOR mode initialized.\r\n");
#endif
#endif
            break;

        default:
            ctx->usart1_initialized = false;
            break;
    }

    /* Configure USART2 (always RS485 for Modbus) */
    if (ctx->usart2_mode == COMM_PATH_MODE_RS485) {
        bsp_rs485_init();
        ctx->usart2_initialized = true;
    }

#ifdef USART1_DEBUG_MODE
    bsp_debug_write_str("[UART_MGR] USART1: ");
    bsp_debug_write_str((char*)get_mode_string(ctx->usart1_mode));
    bsp_debug_write_str(" @ ");
    bsp_debug_write_int("", (int32_t)ctx->usart1_baudrate, " bps\r\n");
    
    bsp_debug_write_str("[UART_MGR] USART2: ");
    bsp_debug_write_str((char*)get_mode_string(ctx->usart2_mode));
    bsp_debug_write_str(" @ ");
    bsp_debug_write_int("", (int32_t)ctx->usart2_baudrate, " bps\r\n");
#endif

    return STATUS_OK;
}

comm_path_mode_e uart_manager_get_usart1_mode(const uart_manager_ctx_s *ctx) {
    if (ctx == NULL) {
        return COMM_PATH_MODE_DISABLED;
    }
    return ctx->usart1_mode;
}

comm_path_mode_e uart_manager_get_usart2_mode(const uart_manager_ctx_s *ctx) {
    if (ctx == NULL) {
        return COMM_PATH_MODE_DISABLED;
    }
    return ctx->usart2_mode;
}

status_e uart_manager_set_usart1_mode(uart_manager_ctx_s *ctx, comm_path_mode_e new_mode) {
    if (ctx == NULL) {
        return STATUS_ERR_GENERIC;
    }

    /* Note: Runtime mode switching requires hardware reconfiguration */
    /* This is a placeholder for future implementation */
    ctx->usart1_mode = new_mode;
    ctx->usart1_baudrate = get_baudrate_for_mode(new_mode);

#ifdef USART1_DEBUG_MODE
    bsp_debug_write_str("[UART_MGR] Mode change requested. Requires reinitialization.\r\n");
#endif

    return STATUS_ERR_GENERIC; /* Not implemented - requires full reinit */
}

void uart_manager_get_stats(const uart_manager_ctx_s *ctx, uint8_t usart_id,
                            uint32_t *tx_count, uint32_t *rx_count, uint32_t *error_count) {
    if (ctx == NULL || tx_count == NULL || rx_count == NULL || error_count == NULL) {
        return;
    }

    switch (usart_id) {
        case 1:
            *tx_count = ctx->usart1_tx_count;
            *rx_count = ctx->usart1_rx_count;
            *error_count = ctx->usart1_errors;
            break;
        case 2:
            *tx_count = ctx->usart2_tx_count;
            *rx_count = ctx->usart2_rx_count;
            *error_count = ctx->usart2_errors;
            break;
        default:
            *tx_count = 0;
            *rx_count = 0;
            *error_count = 0;
            break;
    }
}

void uart_manager_reset_stats(uart_manager_ctx_s *ctx, uint8_t usart_id) {
    if (ctx == NULL) {
        return;
    }

    if (usart_id == 0 || usart_id == 1) {
        ctx->usart1_tx_count = 0;
        ctx->usart1_rx_count = 0;
        ctx->usart1_errors = 0;
    }

    if (usart_id == 0 || usart_id == 2) {
        ctx->usart2_tx_count = 0;
        ctx->usart2_rx_count = 0;
        ctx->usart2_errors = 0;
    }

#ifdef USART1_DEBUG_MODE
    bsp_debug_write_str("[UART_MGR] Statistics reset.\r\n");
#endif
}

const char* uart_manager_get_mode_name(comm_path_mode_e mode) {
    return get_mode_string(mode);
}

uint32_t uart_manager_get_baudrate_for_mode(comm_path_mode_e mode) {
    return get_baudrate_for_mode(mode);
}

/*
 * @file uart_manager.h
 * @brief Centralized UART/Communication Path Configuration
 * 
 * Created on: 15 July 2026
 *     Author: DST0x
 * 
 * Description:
 *   This module provides centralized configuration for all UART communication
 *   paths including RS485, RS232, TTL, and SDI-12 modes. Users can select
 *   the communication protocol from this single configuration file.
 */

#ifndef UART_MANAGER_H
#define UART_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "common/common_types.h"

/* ========================================================================
 * COMMUNICATION MODE SELECTION
 * ======================================================================== */
/* Use #define instead of enum so preprocessor #if comparisons work */
#define COMM_PATH_MODE_RS485    0
#define COMM_PATH_MODE_RS232    1
#define COMM_PATH_MODE_TTL      2
#define COMM_PATH_MODE_SDI12    3
#define COMM_PATH_MODE_DISABLED 4

/* Typedef for use as variable type in structs */
typedef uint8_t comm_path_mode_e;

#define USART1_MODE             COMM_PATH_MODE_RS485  /* Change this to select USART1 mode */

#if (USART1_MODE == COMM_PATH_MODE_TTL)
    #define USART1_DEBUG_MODE
    #undef USART1_MODBUS_MODE
    #undef USART1_SENSOR_MODE
#elif (USART1_MODE == COMM_PATH_MODE_RS485) || (USART1_MODE == COMM_PATH_MODE_RS232)
    #define USART1_MODBUS_MODE
    #undef USART1_DEBUG_MODE
    #undef USART1_SENSOR_MODE
#elif (USART1_MODE == COMM_PATH_MODE_SDI12)
    #define USART1_SENSOR_MODE
    #undef USART1_DEBUG_MODE
    #undef USART1_MODBUS_MODE
#endif

#define USART2_MODE             COMM_PATH_MODE_RS485

#define COMM_RS485_BAUDRATE     9600U
#define COMM_RS232_BAUDRATE     9600U
#define COMM_TTL_BAUDRATE       115200U
#define COMM_SDI12_BAUDRATE     1200U

#define COMM_RS485_DE_ASSERT_DELAY_US   50U     /* DE pin timing */
#define COMM_RS485_DE_NEGATE_DELAY_US   50U

/* ========================================================================
 * BUFFER SIZES
 * ======================================================================== */
#define UART_TX_BUFFER_SIZE     256U
#define UART_RX_BUFFER_SIZE     256U
#define MODBUS_BUFFER_SIZE      64U

/* ========================================================================
 * UART MANAGER CONTEXT
 * ======================================================================== */
typedef struct {
    comm_path_mode_e usart1_mode;
    comm_path_mode_e usart2_mode;
    
    uint32_t usart1_baudrate;
    uint32_t usart2_baudrate;
    
    bool usart1_initialized;
    bool usart2_initialized;
    
    uint32_t usart1_tx_count;
    uint32_t usart1_rx_count;
    uint32_t usart2_tx_count;
    uint32_t usart2_rx_count;
    
    uint32_t usart1_errors;
    uint32_t usart2_errors;
} uart_manager_ctx_s;

/* ========================================================================
 * UART MANAGER API
 * ======================================================================== */

/**
 * @brief Initialize UART manager and configure all communication paths
 * @param ctx Pointer to UART manager context
 * @return STATUS_OK on success
 */
status_e uart_manager_init(uart_manager_ctx_s *ctx);

/**
 * @brief Get current USART1 communication mode
 * @param ctx Pointer to UART manager context
 * @return Current communication mode
 */
comm_path_mode_e uart_manager_get_usart1_mode(const uart_manager_ctx_s *ctx);

/**
 * @brief Get current USART2 communication mode
 * @param ctx Pointer to UART manager context
 * @return Current communication mode
 */
comm_path_mode_e uart_manager_get_usart2_mode(const uart_manager_ctx_s *ctx);

/**
 * @brief Change USART1 mode at runtime (requires reinitialization)
 * @param ctx Pointer to UART manager context
 * @param new_mode New communication mode
 * @return STATUS_OK on success
 */
status_e uart_manager_set_usart1_mode(uart_manager_ctx_s *ctx, comm_path_mode_e new_mode);

/**
 * @brief Get UART statistics
 * @param ctx Pointer to UART manager context
 * @param usart_id USART identifier (1 or 2)
 * @param tx_count Output: transmission count
 * @param rx_count Output: reception count
 * @param error_count Output: error count
 */
void uart_manager_get_stats(const uart_manager_ctx_s *ctx, uint8_t usart_id,
                            uint32_t *tx_count, uint32_t *rx_count, uint32_t *error_count);

/**
 * @brief Reset UART statistics
 * @param ctx Pointer to UART manager context
 * @param usart_id USART identifier (1 or 2, or 0 for all)
 */
void uart_manager_reset_stats(uart_manager_ctx_s *ctx, uint8_t usart_id);

/**
 * @brief Get mode name as string
 * @param mode Communication mode
 * @return Mode name string
 */
const char* uart_manager_get_mode_name(comm_path_mode_e mode);

/**
 * @brief Get recommended baudrate for mode
 * @param mode Communication mode
 * @return Baudrate in bps
 */
uint32_t uart_manager_get_baudrate_for_mode(comm_path_mode_e mode);

#endif /* UART_MANAGER_H */

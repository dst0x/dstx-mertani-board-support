/*
* @file infwin_co_sensor.h
* 
* Created on: 14 June 2026
*     Author: DST0x
*/

#ifndef INFWIN_CO_SENSOR_H
#define INFWIN_CO_SENSOR_H

/* Global Includes */
#include <stddef.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* Local Includes */
#include "common/common_types.h"
#include "bsp/bsp_uart.h"
#include "bsp/bsp_systick.h"
#include "../../middleware/modbus/modbus_crc.h"

#define INFWIN_CO_SENSOR_ADDR           (0x62U)
#define INFWIN_CO_REQUEST_INTERVAL_MS   (1000U)
#define INFWIN_CO_RESPONSE_TIMEOUT_MS   (500U)
#define INFWIN_CO_MAX_ERRORS            (5U)
#define INFWIN_CO_REQUEST_LEN           (8U)
#define INFWIN_CO_RESPONSE_LEN          (7U)

#define INFWIN_CO_UNAVAIL_U16           (0xFFFEU)
#define INFWIN_CO_INIT_U16              (0xFFFEU)

typedef enum co_state_tag{
    CO_STATE_UNINIT     = 0,
    CO_STATE_IDLE       = 1,
    CO_STATE_WARMING_UP = 2,
    CO_STATE_RUNNING    = 3,
    CO_STATE_ERROR      = 4
} co_state_e;

typedef struct co_data_tag{
    uint32_t request_count;
    uint32_t response_count;
    uint32_t timeout_count;
    uint32_t crc_error_count;
    
    uint16_t co_value;  
    bool valid;        
} co_data_s;

typedef struct co_ctx_tag{
    co_data_s  data;
    co_data_s  last_valid_data;
    
    uint32_t   last_request_tick;
    uint32_t   request_sent_tick;
    
    co_state_e state;
    
    uint8_t    error_count;
    uint8_t    rx_index;
    bool       data_fresh;
    bool       has_valid_data;
    bool       waiting_response;
    
    uint8_t    rx_buf[INFWIN_CO_RESPONSE_LEN];
    uint8_t    last_response[INFWIN_CO_RESPONSE_LEN];
} co_ctx_s;

#ifdef USART1_MODBUS_MODE
    status_e sensor_co_init(co_ctx_s *ctx);
    status_e sensor_co_send_request(co_ctx_s *ctx);
    status_e sensor_co_poll(co_ctx_s *ctx);
    co_state_e sensor_co_get_state(const co_ctx_s *ctx);

    void sensor_co_rx_byte(co_ctx_s *ctx, uint8_t byte);
    const uint8_t* sensor_co_get_last_resp(const co_ctx_s *ctx);
#endif
#endif
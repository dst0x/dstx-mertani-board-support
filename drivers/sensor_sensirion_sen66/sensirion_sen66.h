/*
* @file sensirion_sen66.h
* 
* Created on: 14 June 2026
*     Author: DST0x
* 
* Modified: 15 July 2026
*     Integrated with sensor_manager for centralized control
* 
* Register map:
*   [0] PM1.0  ÷10 µg/m³
*   [1] PM2.5  ÷10 µg/m³
*   [2] PM4.0  ÷10 µg/m³
*   [3] PM10   ÷10 µg/m³
*   [4] RH     ÷100 %RH
*   [5] Temp   ÷10 °C
*   [6] VOC    ÷10
*   [7] NOx    ÷10
*   [8] CO2    ppm
* 
* Integration Notes:
*   - This driver is designed to work with sensor_manager middleware
*   - Enable/disable via SENSOR_ENABLE_SEN66 in sensor_manager.h
*   - Automatic error recovery and retry handled by sensor_manager
*   - Zero value filtering applied by sensor_manager when enabled
*   - Periodic reset scheduled by sensor_manager
*/

#ifndef SENSIRION_SEN66_H
#define SENSIRION_SEN66_H

#include <stddef.h>
#include <string.h>

#include "common/common_types.h"
#include "bsp/bsp_i2c.h"
#include "bsp/bsp_systick.h"

#define SEN66_I2C_ADDR         (0x6BU)
#define SEN66_CMD_START_MEAS   (0x0021U)
#define SEN66_CMD_STOP_MEAS    (0x0104U)
#define SEN66_CMD_DATA_READY   (0x0202U)
#define SEN66_CMD_READ_VALUES  (0x0300U)
#define SEN66_CMD_READ_PM_NC   (0x0316U)
#define SEN66_CMD_READ_PRODUCT (0xD014U)
#define SEN66_CMD_RESET        (0xD304U)

#define SEN66_STARTUP_MS       (100U)
#define SEN66_READ_DELAY_MS    (20U)
#define SEN66_STOP_DELAY_MS    (1400U)
#define SEN66_RESET_DELAY_MS   (1200U)
#define SEN66_WARMUP_MS        (500UL)

#define SEN66_MEAS_PAYLOAD_BYTES (27U)
#define SEN66_NC_PAYLOAD_BYTES   (15U)
#define SEN66_MAX_ERRORS         (5U)

#define SEN66_UNAVAIL_U16  (0xFFFFU)
#define SEN66_INIT_U16     (0xFFFEU)
#define SEN66_UNAVAIL_I16  ((int16_t)0x7FFF)

typedef enum sen66_state_tag{
    SEN66_STATE_UNINIT     = 0,
    SEN66_STATE_IDLE       = 1,
    SEN66_STATE_WARMING_UP = 2,
    SEN66_STATE_RUNNING    = 3,
    SEN66_STATE_ERROR      = 4
}sen66_state_e;

typedef struct data_tag{
    uint16_t pm1_0;
    uint16_t pm2_5;
    uint16_t pm4_0;
    uint16_t pm10;
    uint16_t nc0_5;
    uint16_t nc1_0;
    uint16_t nc2_5;
    uint16_t nc4_0;
    uint16_t nc10;
    uint16_t typ_size;
    int16_t  rh;
    int16_t  temp;
    int16_t  voc;
    int16_t  nox;
    uint16_t co2_ppm;
    bool     pm_valid;
    bool     env_valid;
    bool     gas_valid;
    bool     co2_valid;
}sen66_data_s;

typedef struct ctx_tag{
    uint32_t      start_tick;
    uint32_t      last_read_tick;
    sen66_state_e state;
    sen66_data_s  data;
    sen66_data_s  last_valid_data;  
    uint8_t       error_count;
    bool          data_fresh;
    bool          has_valid_data;
    uint8_t       raw_meas[SEN66_MEAS_PAYLOAD_BYTES];
}sen66_ctx_s;

status_e sensirion_sen66_init(sen66_ctx_s *ctx);
status_e sensirion_sen66_start_measurement(sen66_ctx_s *ctx);
status_e sensirion_sen66_stop_measurement(sen66_ctx_s *ctx);
status_e sensirion_sen66_poll(sen66_ctx_s *ctx);
status_e sensirion_sen66_reset(sen66_ctx_s *ctx);

sen66_state_e sensirion_sen66_get_state(const sen66_ctx_s *ctx);
uint8_t sensirion_sen66_calc_crc(const uint8_t data[2U]);
bool sensirion_sen66_is_data_ready(sen66_ctx_s *ctx);

/* Utility functions for sensor_manager integration */
static inline bool sensirion_sen66_has_valid_data(const sen66_ctx_s *ctx) {
    return (ctx != NULL) && ctx->has_valid_data;
}

static inline bool sensirion_sen66_is_warming_up(const sen66_ctx_s *ctx) {
    return (ctx != NULL) && (ctx->state == SEN66_STATE_WARMING_UP);
}

static inline bool sensirion_sen66_is_running(const sen66_ctx_s *ctx) {
    return (ctx != NULL) && (ctx->state == SEN66_STATE_RUNNING);
}

static inline bool sensirion_sen66_has_error(const sen66_ctx_s *ctx) {
    return (ctx != NULL) && (ctx->state == SEN66_STATE_ERROR);
}

static inline uint8_t sensirion_sen66_get_error_count(const sen66_ctx_s *ctx) {
    return (ctx != NULL) ? ctx->error_count : 0U;
}

#endif /* SENSIRION_SEN66_H */
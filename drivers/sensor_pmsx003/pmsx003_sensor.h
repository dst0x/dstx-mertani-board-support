/*
 * @file pmsx003_sensor.h
 * 
 * Created on: 15 July 2026
 *     Author: DST0x
 * 
 */

#ifndef PMSX003_SENSOR_H
#define PMSX003_SENSOR_H

#include "common/common_types.h"

#define PMSX003_START1           (0x42U)
#define PMSX003_START2           (0x4DU)
#define PMSX003_FRAME_LEN        (32U)
#define PMSX003_DATA_LEN         (28U)  
#define PMSX003_RX_TIMEOUT_MS    (3000U)
#define PMSX003_MAX_ERRORS       (10U)

typedef enum pmsx003_state_tag {
    PMSX003_STATE_UNINIT    = 0,
    PMSX003_STATE_IDLE      = 1,
    PMSX003_STATE_RECEIVING = 2,
    PMSX003_STATE_READY     = 3,
    PMSX003_STATE_ERROR     = 4
} pmsx003_state_e;

typedef struct pmsx003_data_tag{
    uint16_t tsp_ug_m3;       /* TSP concentration (µg/m³) */
    uint16_t pm1_0_ug_m3;     /* PM1.0 concentration (µg/m³) */
    uint16_t pm2_5_ug_m3;     /* PM2.5 concentration (µg/m³) */
    uint16_t pm10_ug_m3;      /* PM10 concentration (µg/m³) */
    uint16_t count_0_3;       /* Particles >0.3µm in 0.1L air */
    uint16_t count_0_5;       /* Particles >0.5µm in 0.1L air */
    uint16_t count_1_0;       /* Particles >1.0µm in 0.1L air */
    uint16_t count_2_5;       /* Particles >2.5µm in 0.1L air */
    uint16_t count_5_0;       /* Particles >5.0µm in 0.1L air */
    uint16_t count_10;        /* Particles >10µm in 0.1L air */
    uint8_t  version;         /* Firmware version */
    uint8_t  error_code;      /* Sensor error code */
} pmsx003_data_s;

typedef struct pmsx003_ctx_tag {
    pmsx003_state_e state;
    pmsx003_data_s  data;
    uint8_t         rx_buf[PMSX003_FRAME_LEN];
    uint8_t         rx_idx;
    uint32_t        last_rx_tick;
    uint32_t        last_valid_tick;
    bool            data_fresh;
    uint16_t        error_count;
} pmsx003_ctx_s;

pmsx003_state_e sensor_pmsx003_get_state(const pmsx003_ctx_s *ctx);

status_e sensor_pmsx003_init(pmsx003_ctx_s *ctx);
status_e sensor_pmsx003_poll(pmsx003_ctx_s *ctx);
status_e sensor_pmsx003_reset(pmsx003_ctx_s *ctx);

void sensor_pmsx003_rx_byte(pmsx003_ctx_s *ctx, uint8_t byte);
void sensor_pmsx003_clear_fresh(pmsx003_ctx_s *ctx);

const pmsx003_data_s* sensor_pmsx003_get_data(const pmsx003_ctx_s *ctx);

bool sensor_pmsx003_is_data_fresh(const pmsx003_ctx_s *ctx);

#endif /* PMSX003_SENSOR_H */
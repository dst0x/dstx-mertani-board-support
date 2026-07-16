/*
 * @file sensor_manager.h
 * @brief Centralized Sensor Configuration and Management
 * 
 * Created on: 15 July 2026
 *     Author: DST0x
 * 
 * Description:
 *   This module provides centralized configuration and management for all sensors
 *   in the system. Users can enable/disable sensors and configure filtering,
 *   reset intervals, and retry logic from this single interface.
 */

#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "common/common_types.h"

#define SENSOR_ENABLE_SEN66         1   /* Sensirion SEN66 Air Quality Sensor */
#define SENSOR_ENABLE_INFWIN_CO     1   /* Infwin CO Sensor */

#define SENSOR_POLL_INTERVAL_MS     1000U   /* Normal polling interval */
#define SENSOR_RETRY_INTERVAL_MS    5000U   /* Retry interval after error */
#define SENSOR_RESET_INTERVAL_MS    3600000U /* Periodic reset (1 hour) */

#define SENSOR_ENABLE_ZERO_FILTER   1       /* Filter out zero readings */
#define SENSOR_MAX_ZERO_COUNT       3       /* Max consecutive zeros before using last valid */
#define SENSOR_MAX_ERRORS           5       /* Max errors before sensor reset */

#define SENSOR_SEN66_WARMUP_MS      30000U  /* SEN66 warmup time: 30 seconds (default 60s) */
#define SENSOR_CO_WARMUP_MS         30000U  /* CO sensor warmup time */

typedef enum {
    SENSOR_STATUS_UNINITIALIZED = 0,
    SENSOR_STATUS_INITIALIZING,
    SENSOR_STATUS_WARMING_UP,
    SENSOR_STATUS_READY,
    SENSOR_STATUS_RUNNING,
    SENSOR_STATUS_ERROR,
    SENSOR_STATUS_DISABLED
} sensor_status_e;

typedef struct {
    sensor_status_e status;
    uint32_t last_poll_tick;
    uint32_t last_reset_tick;
    uint32_t error_count;
    uint32_t zero_count;
    bool data_valid;
    bool has_valid_data;
} sensor_common_ctx_s;

typedef struct {
#if SENSOR_ENABLE_SEN66
    sensor_common_ctx_s sen66_ctx;
    void *sen66_driver;  /* Pointer to sen66_ctx_s */
#endif

#if SENSOR_ENABLE_INFWIN_CO
    sensor_common_ctx_s co_ctx;
    void *co_driver;     /* Pointer to co_ctx_s */
#endif

    uint32_t global_tick;
    bool initialized;
} sensor_manager_ctx_s;


/**
 * @brief Initialize sensor manager and all enabled sensors
 * @param ctx Pointer to sensor manager context
 * @return STATUS_OK on success, error code otherwise
 */
status_e sensor_manager_init(sensor_manager_ctx_s *ctx);

/**
 * @brief Poll all enabled sensors (call in main loop)
 * @param ctx Pointer to sensor manager context
 * @return STATUS_OK if at least one sensor updated successfully
 */
status_e sensor_manager_poll(sensor_manager_ctx_s *ctx);

/**
 * @brief Get status of a specific sensor
 * @param ctx Pointer to sensor manager context
 * @param sensor_id Sensor identifier (0=SEN66, 1=CO)
 * @return Current sensor status
 */
sensor_status_e sensor_manager_get_status(sensor_manager_ctx_s *ctx, uint8_t sensor_id);

/**
 * @brief Force reset of a specific sensor
 * @param ctx Pointer to sensor manager context
 * @param sensor_id Sensor identifier (0=SEN66, 1=CO)
 * @return STATUS_OK on success
 */
status_e sensor_manager_reset_sensor(sensor_manager_ctx_s *ctx, uint8_t sensor_id);

/**
 * @brief Apply zero filtering to sensor reading
 * @param current_value Current sensor reading
 * @param last_valid_value Last known valid reading
 * @param zero_count Pointer to zero counter
 * @return Filtered value (current or last valid)
 */
int16_t sensor_manager_filter_zero(int16_t current_value, int16_t last_valid_value, uint32_t *zero_count);

/**
 * @brief Check if periodic reset is needed
 * @param last_reset_tick Last reset timestamp
 * @param current_tick Current timestamp
 * @return true if reset is needed
 */
bool sensor_manager_needs_periodic_reset(uint32_t last_reset_tick, uint32_t current_tick);

/**
 * @brief Get sensor manager statistics
 * @param ctx Pointer to sensor manager context
 * @param sensor_id Sensor identifier
 * @param error_count Output: error count
 * @param zero_count Output: zero reading count
 */
void sensor_manager_get_stats(sensor_manager_ctx_s *ctx, uint8_t sensor_id, 
                              uint32_t *error_count, uint32_t *zero_count);

#endif /* SENSOR_MANAGER_H */

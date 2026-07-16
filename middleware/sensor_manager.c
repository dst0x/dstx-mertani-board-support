/*
 * @file sensor_manager.c
 * @brief Centralized Sensor Configuration and Management
 * 
 * Created on: 15 July 2026
 *     Author: DST0x
 */

#include "sensor_manager.h"
#include "bsp/bsp_systick.h"
#include "bsp/bsp_iwdg.h"

#if SENSOR_ENABLE_SEN66
#include "drivers/sensor_sensirion_sen66/sensirion_sen66.h"
#endif

#if SENSOR_ENABLE_INFWIN_CO
#include "drivers/sensor_infwin_co/infwin_co_sensor.h"
#endif

#ifdef USART1_DEBUG_MODE
#include "bsp/bsp_uart.h"
#endif

static bool is_reading_zero(int16_t value) {
    return (value == 0 || value == 0xFFFF || value == 0xFFFE);
}

static bool is_warmup_complete(uint32_t start_tick, uint32_t warmup_ms) {
    return bsp_systick_elapsed(start_tick, warmup_ms);
}

status_e sensor_manager_init(sensor_manager_ctx_s *ctx) {
    if (ctx == NULL) {
        return STATUS_ERR_GENERIC;
    }

    ctx->initialized = false;
    ctx->global_tick = bsp_systick_get_tick();

#if SENSOR_ENABLE_SEN66
    ctx->sen66_ctx.status = SENSOR_STATUS_INITIALIZING;
    ctx->sen66_ctx.error_count = 0;
    ctx->sen66_ctx.zero_count = 0;
    ctx->sen66_ctx.data_valid = false;
    ctx->sen66_ctx.has_valid_data = false;
    ctx->sen66_ctx.last_poll_tick = ctx->global_tick;
    ctx->sen66_ctx.last_reset_tick = ctx->global_tick;

    sen66_ctx_s *sen66 = (sen66_ctx_s *)ctx->sen66_driver;
    if (sen66 != NULL) {
        status_e status;
        uint8_t attempt;
        
        for (attempt = 0; attempt < 3; attempt++) {
            bsp_iwdg_refresh();
            status = sensirion_sen66_init(sen66);
            if (status == STATUS_OK) {
                break;
            }
#ifdef USART1_DEBUG_MODE
            bsp_debug_write_str("[SENSOR_MGR] SEN66 init retry...\r\n");
#endif
            bsp_systick_delay_ms(500);
        }

        if (status == STATUS_OK) {
            status = sensirion_sen66_start_measurement(sen66);
            if (status == STATUS_OK) {
                ctx->sen66_ctx.status = SENSOR_STATUS_RUNNING;  /* Skip warmup, start reading immediately */
#ifdef USART1_DEBUG_MODE
                bsp_debug_write_str("[SENSOR_MGR] SEN66 ready.\r\n");
#endif
            } else {
                ctx->sen66_ctx.status = SENSOR_STATUS_ERROR;
#ifdef USART1_DEBUG_MODE
                bsp_debug_write_str("[SENSOR_MGR] SEN66 start measurement failed.\r\n");
#endif
            }
        } else {
            ctx->sen66_ctx.status = SENSOR_STATUS_ERROR;
#ifdef USART1_DEBUG_MODE
            bsp_debug_write_str("[SENSOR_MGR] SEN66 init failed.\r\n");
#endif
        }
    }
#endif

#if SENSOR_ENABLE_INFWIN_CO
    ctx->co_ctx.status = SENSOR_STATUS_INITIALIZING;
    ctx->co_ctx.error_count = 0;
    ctx->co_ctx.zero_count = 0;
    ctx->co_ctx.data_valid = false;
    ctx->co_ctx.has_valid_data = false;
    ctx->co_ctx.last_poll_tick = ctx->global_tick;
    ctx->co_ctx.last_reset_tick = ctx->global_tick;

#ifdef USART1_MODBUS_MODE
    co_ctx_s *co = (co_ctx_s *)ctx->co_driver;
    if (co != NULL) {
        status_e status = sensor_co_init(co);
        if (status == STATUS_OK) {
            ctx->co_ctx.status = SENSOR_STATUS_RUNNING;  /* Skip warmup, start reading immediately */
#ifdef USART1_DEBUG_MODE
            bsp_debug_write_str("[SENSOR_MGR] CO sensor ready.\r\n");
#endif
        } else {
            ctx->co_ctx.status = SENSOR_STATUS_ERROR;
#ifdef USART1_DEBUG_MODE
            bsp_debug_write_str("[SENSOR_MGR] CO sensor init failed.\r\n");
#endif
        }
    }
#endif
#endif

    ctx->initialized = true;
    return STATUS_OK;
}

status_e sensor_manager_poll(sensor_manager_ctx_s *ctx) {
    if (ctx == NULL || !ctx->initialized) {
        return STATUS_ERR_GENERIC;
    }

    status_e overall_status = STATUS_ERR_GENERIC;
    uint32_t now = bsp_systick_get_tick();
    ctx->global_tick = now;

#if SENSOR_ENABLE_SEN66
    sen66_ctx_s *sen66 = (sen66_ctx_s *)ctx->sen66_driver;
    if (sen66 != NULL) {
        /* Handle running state */
        if (ctx->sen66_ctx.status == SENSOR_STATUS_RUNNING) {
            if (bsp_systick_elapsed(ctx->sen66_ctx.last_poll_tick, SENSOR_POLL_INTERVAL_MS)) {
                ctx->sen66_ctx.last_poll_tick = now;

                if (sensirion_sen66_is_data_ready(sen66)) {
                    status_e status = sensirion_sen66_poll(sen66);

                    if (status == STATUS_OK) {
                        ctx->sen66_ctx.error_count = 0;
                        ctx->sen66_ctx.data_valid = true;
                        ctx->sen66_ctx.has_valid_data = true;
                        overall_status = STATUS_OK;

#if SENSOR_ENABLE_ZERO_FILTER
                        /* Apply zero filtering */
                        if (is_reading_zero(sen66->data.pm2_5)) {
                            ctx->sen66_ctx.zero_count++;
                            if (ctx->sen66_ctx.zero_count > SENSOR_MAX_ZERO_COUNT && ctx->sen66_ctx.has_valid_data) {
                                /* Use last valid data */
                                sen66->data = sen66->last_valid_data;
                            }
                        } else {
                            ctx->sen66_ctx.zero_count = 0;
                            sen66->last_valid_data = sen66->data;
                        }
#endif
                    } else if (status != STATUS_NOT_READY && status != STATUS_ERR_STATE) {
                        ctx->sen66_ctx.error_count++;
                        if (ctx->sen66_ctx.error_count >= SENSOR_MAX_ERRORS) {
                            ctx->sen66_ctx.status = SENSOR_STATUS_ERROR;
                            ctx->sen66_ctx.data_valid = false;
#ifdef USART1_DEBUG_MODE
                            bsp_debug_write_str("[SENSOR_MGR] SEN66 error threshold exceeded.\r\n");
#endif
                        }
                    }
                }
            }

            /* Check for periodic reset */
            if (sensor_manager_needs_periodic_reset(ctx->sen66_ctx.last_reset_tick, now)) {
#ifdef USART1_DEBUG_MODE
                bsp_debug_write_str("[SENSOR_MGR] SEN66 periodic reset.\r\n");
#endif
                sensor_manager_reset_sensor(ctx, 0);
            }
        }

        /* Handle error state - retry */
        if (ctx->sen66_ctx.status == SENSOR_STATUS_ERROR) {
            if (bsp_systick_elapsed(ctx->sen66_ctx.last_poll_tick, SENSOR_RETRY_INTERVAL_MS)) {
                ctx->sen66_ctx.last_poll_tick = now;
                bsp_iwdg_refresh();

#ifdef USART1_DEBUG_MODE
                bsp_debug_write_str("[SENSOR_MGR] Attempting to recover SEN66...\r\n");
#endif

                status_e status = sensirion_sen66_init(sen66);
                if (status == STATUS_OK) {
                    status = sensirion_sen66_start_measurement(sen66);
                    if (status == STATUS_OK) {
                        ctx->sen66_ctx.status = SENSOR_STATUS_RUNNING;  /* Skip warmup after recovery */
                        ctx->sen66_ctx.error_count = 0;
                        ctx->sen66_ctx.last_reset_tick = now;
#ifdef USART1_DEBUG_MODE
                        bsp_debug_write_str("[SENSOR_MGR] SEN66 reconnected.\r\n");
#endif
                    } else {
#ifdef USART1_DEBUG_MODE
                        bsp_debug_write_str("[SENSOR_MGR] SEN66 start measurement failed during recovery.\r\n");
#endif
                    }
                } else {
#ifdef USART1_DEBUG_MODE
                    bsp_debug_write_str("[SENSOR_MGR] SEN66 init failed during recovery.\r\n");
#endif
                }
                bsp_iwdg_refresh();
            }
        }
    }
#endif

#if SENSOR_ENABLE_INFWIN_CO
#ifdef USART1_MODBUS_MODE
    co_ctx_s *co = (co_ctx_s *)ctx->co_driver;
    if (co != NULL) {
        /* Poll CO sensor directly when RUNNING */
        if (ctx->co_ctx.status == SENSOR_STATUS_RUNNING) {
            status_e status = sensor_co_poll(co);
            
            if (status == STATUS_OK && co->data.valid) {
                ctx->co_ctx.error_count = 0;
                ctx->co_ctx.data_valid = true;
                ctx->co_ctx.has_valid_data = true;
                overall_status = STATUS_OK;

#if SENSOR_ENABLE_ZERO_FILTER
                if (is_reading_zero(co->data.co_value)) {
                    ctx->co_ctx.zero_count++;
                    if (ctx->co_ctx.zero_count > SENSOR_MAX_ZERO_COUNT && ctx->co_ctx.has_valid_data) {
                        co->data = co->last_valid_data;
                    }
                } else {
                    ctx->co_ctx.zero_count = 0;
                    co->last_valid_data = co->data;
                }
#endif
            } else if (status != STATUS_OK) {
                ctx->co_ctx.error_count++;
                if (ctx->co_ctx.error_count >= SENSOR_MAX_ERRORS) {
                    ctx->co_ctx.status = SENSOR_STATUS_ERROR;
                    ctx->co_ctx.data_valid = false;
#ifdef USART1_DEBUG_MODE
                    bsp_debug_write_str("[SENSOR_MGR] CO sensor error threshold exceeded.\r\n");
#endif
                }
            }
        }

        /* Handle error state - retry CO sensor init */
        if (ctx->co_ctx.status == SENSOR_STATUS_ERROR) {
            if (bsp_systick_elapsed(ctx->co_ctx.last_poll_tick, SENSOR_RETRY_INTERVAL_MS)) {
                ctx->co_ctx.last_poll_tick = now;
                bsp_iwdg_refresh();

#ifdef USART1_DEBUG_MODE
                bsp_debug_write_str("[SENSOR_MGR] Attempting to recover CO sensor...\r\n");
#endif

                status_e status = sensor_co_init(co);
                if (status == STATUS_OK) {
                    ctx->co_ctx.status = SENSOR_STATUS_RUNNING;
                    ctx->co_ctx.error_count = 0;
                    ctx->co_ctx.last_reset_tick = now;
#ifdef USART1_DEBUG_MODE
                    bsp_debug_write_str("[SENSOR_MGR] CO sensor reconnected.\r\n");
#endif
                } else {
#ifdef USART1_DEBUG_MODE
                    bsp_debug_write_str("[SENSOR_MGR] CO sensor init failed during recovery.\r\n");
#endif
                }
                bsp_iwdg_refresh();
            }
        }

        /* Check for periodic reset */
        if (ctx->co_ctx.status == SENSOR_STATUS_RUNNING) {
            if (sensor_manager_needs_periodic_reset(ctx->co_ctx.last_reset_tick, now)) {
#ifdef USART1_DEBUG_MODE
                bsp_debug_write_str("[SENSOR_MGR] CO sensor periodic reset.\r\n");
#endif
                sensor_manager_reset_sensor(ctx, 1);
            }
        }
    }
#endif
#endif

    return overall_status;
}

sensor_status_e sensor_manager_get_status(sensor_manager_ctx_s *ctx, uint8_t sensor_id) {
    if (ctx == NULL) {
        return SENSOR_STATUS_DISABLED;
    }

    switch (sensor_id) {
#if SENSOR_ENABLE_SEN66
        case 0:
            return ctx->sen66_ctx.status;
#endif
#if SENSOR_ENABLE_INFWIN_CO
        case 1:
            return ctx->co_ctx.status;
#endif
        default:
            return SENSOR_STATUS_DISABLED;
    }
}

status_e sensor_manager_reset_sensor(sensor_manager_ctx_s *ctx, uint8_t sensor_id) {
    if (ctx == NULL) {
        return STATUS_ERR_GENERIC;
    }

    uint32_t now = bsp_systick_get_tick();

    switch (sensor_id) {
#if SENSOR_ENABLE_SEN66
        case 0: {
            sen66_ctx_s *sen66 = (sen66_ctx_s *)ctx->sen66_driver;
            if (sen66 != NULL) {
                bsp_iwdg_refresh();
                status_e status = sensirion_sen66_reset(sen66);
                bsp_systick_delay_ms(SEN66_RESET_DELAY_MS);
                
                if (status == STATUS_OK) {
                    status = sensirion_sen66_start_measurement(sen66);
                    if (status == STATUS_OK) {
                        ctx->sen66_ctx.status = SENSOR_STATUS_RUNNING;  /* Skip warmup after reset */
                        ctx->sen66_ctx.error_count = 0;
                        ctx->sen66_ctx.last_reset_tick = now;
                        bsp_iwdg_refresh();
                        return STATUS_OK;
                    }
                }
                bsp_iwdg_refresh();
            }
            return STATUS_ERR_GENERIC;
        }
#endif
#if SENSOR_ENABLE_INFWIN_CO
        case 1:
            /* CO sensor doesn't have reset command, reinitialize */
            ctx->co_ctx.status = SENSOR_STATUS_INITIALIZING;
            ctx->co_ctx.error_count = 0;
            ctx->co_ctx.last_reset_tick = now;
            return STATUS_OK;
#endif
        default:
            return STATUS_ERR_GENERIC;
    }
}

int16_t sensor_manager_filter_zero(int16_t current_value, int16_t last_valid_value, uint32_t *zero_count) {
#if SENSOR_ENABLE_ZERO_FILTER
    if (is_reading_zero(current_value)) {
        (*zero_count)++;
        if (*zero_count > SENSOR_MAX_ZERO_COUNT) {
            return last_valid_value;
        }
    } else {
        *zero_count = 0;
    }
#endif
    return current_value;
}

bool sensor_manager_needs_periodic_reset(uint32_t last_reset_tick, uint32_t current_tick) {
    return bsp_systick_elapsed(last_reset_tick, SENSOR_RESET_INTERVAL_MS);
}

void sensor_manager_get_stats(sensor_manager_ctx_s *ctx, uint8_t sensor_id, 
                              uint32_t *error_count, uint32_t *zero_count) {
    if (ctx == NULL || error_count == NULL || zero_count == NULL) {
        return;
    }

    switch (sensor_id) {
#if SENSOR_ENABLE_SEN66
        case 0:
            *error_count = ctx->sen66_ctx.error_count;
            *zero_count = ctx->sen66_ctx.zero_count;
            break;
#endif
#if SENSOR_ENABLE_INFWIN_CO
        case 1:
            *error_count = ctx->co_ctx.error_count;
            *zero_count = ctx->co_ctx.zero_count;
            break;
#endif
        default:
            *error_count = 0;
            *zero_count = 0;
            break;
    }
}

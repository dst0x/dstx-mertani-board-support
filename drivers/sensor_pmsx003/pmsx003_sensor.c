/*
 * @file pmsx003_sensor.c
 * 
 * Created on: 15 July 2026
 *     Author: DST0x
 * 
 */

#include "drivers/sensor_pmsx003/pmsx003_sensor.h"
#include "bsp/bsp_systick.h"
#include "bsp/bsp_iwdg.h"
#include <stddef.h>
#include <string.h>

#ifdef USART1_DEBUG_MODE
#include "bsp/bsp_uart.h"
#endif

static bool validate_checksum(const uint8_t *buf)
{
    uint16_t sum = 0U;
    uint16_t checksum;
    uint8_t i;

    /* Calculate sum of bytes 0..29 */
    for (i = 0U; i < 30U; i++)
    {
        sum += (uint16_t)buf[i];
    }

    /* Extract checksum from frame (bytes 30-31) */
    checksum = ((uint16_t)buf[30U] << 8U) | (uint16_t)buf[31U];

    return (sum == checksum);
}

static void parse_frame(const uint8_t *buf, pmsx003_data_s *data)
{
    /* PMSX003 Protocol (Active Mode, 32 bytes):
     * Byte 0-1:   Start characters (0x42 0x4D)
     * Byte 2-3:   Frame length (0x00 0x1C = 28 bytes)
     * Byte 4-7:   Reserved (PM1.0/2.5/10 CF=1, not used)
     * Byte 8-9:   TSP concentration (µg/m³) under atmospheric environment
     * Byte 10-11: PM1.0 concentration (µg/m³)
     * Byte 12-13: PM2.5 concentration (µg/m³)
     * Byte 14-15: PM10 concentration (µg/m³)
     * Byte 16-17: Particles > 0.3µm in 0.1L air
     * Byte 18-19: Particles > 0.5µm in 0.1L air
     * Byte 20-21: Particles > 1.0µm in 0.1L air
     * Byte 22-23: Particles > 2.5µm in 0.1L air
     * Byte 24-25: Particles > 5.0µm in 0.1L air
     * Byte 26-27: Particles > 10µm in 0.1L air
     * Byte 28:    Firmware version
     * Byte 29:    Error code
     * Byte 30-31: Checksum
     */
    
    data->tsp_ug_m3    = ((uint16_t)buf[8U]  << 8U) | (uint16_t)buf[9U];
    data->pm1_0_ug_m3  = ((uint16_t)buf[10U] << 8U) | (uint16_t)buf[11U];
    data->pm2_5_ug_m3  = ((uint16_t)buf[12U] << 8U) | (uint16_t)buf[13U];
    data->pm10_ug_m3   = ((uint16_t)buf[14U] << 8U) | (uint16_t)buf[15U];
    
    data->count_0_3    = ((uint16_t)buf[16U] << 8U) | (uint16_t)buf[17U];
    data->count_0_5    = ((uint16_t)buf[18U] << 8U) | (uint16_t)buf[19U];
    data->count_1_0    = ((uint16_t)buf[20U] << 8U) | (uint16_t)buf[21U];
    data->count_2_5    = ((uint16_t)buf[22U] << 8U) | (uint16_t)buf[23U];
    data->count_5_0    = ((uint16_t)buf[24U] << 8U) | (uint16_t)buf[25U];
    data->count_10     = ((uint16_t)buf[26U] << 8U) | (uint16_t)buf[27U];
    
    data->version      = buf[28U];
    data->error_code   = buf[29U];
}

status_e sensor_pmsx003_init(pmsx003_ctx_s *ctx)
{
    if (ctx == NULL)
    {
        return STATUS_ERR_PARAM;
    }

    (void)memset(ctx, 0, sizeof(pmsx003_ctx_s));
    
    ctx->state           = PMSX003_STATE_IDLE;
    ctx->rx_idx          = 0U;
    ctx->data_fresh      = false;
    ctx->error_count     = 0U;
    ctx->last_rx_tick    = bsp_systick_get_tick();
    ctx->last_valid_tick = ctx->last_rx_tick;

#ifdef USART1_DEBUG_MODE
    bsp_uart_transmit_string((uint8_t*)"[PMSX003] Init OK\r\n");
#endif

    return STATUS_OK;
}

void sensor_pmsx003_rx_byte(pmsx003_ctx_s *ctx, uint8_t byte)
{
    uint32_t now;
    
    if (ctx == NULL)
    {
        return;
    }

    now = bsp_systick_get_tick();
    ctx->last_rx_tick = now;

    switch (ctx->rx_idx)
    {
        case 0U:
            /* Wait for start byte 1 (0x42) */
            if (byte == PMSX003_START1)
            {
                ctx->rx_buf[ctx->rx_idx] = byte;
                ctx->rx_idx++;
                ctx->state = PMSX003_STATE_RECEIVING;
            }
            break;

        case 1U:
            /* Check start byte 2 (0x4D) */
            if (byte == PMSX003_START2)
            {
                ctx->rx_buf[ctx->rx_idx] = byte;
                ctx->rx_idx++;
            }
            else
            {
                /* Invalid start sequence, reset */
                ctx->rx_idx = 0U;
                ctx->state  = PMSX003_STATE_IDLE;
            }
            break;

        default:
            /* Collect remaining bytes */
            if (ctx->rx_idx < PMSX003_FRAME_LEN)
            {
                ctx->rx_buf[ctx->rx_idx] = byte;
                ctx->rx_idx++;

                if (ctx->rx_idx >= PMSX003_FRAME_LEN)
                {
                    /* Complete frame received, validate checksum */
                    if (validate_checksum(ctx->rx_buf))
                    {
                        parse_frame(ctx->rx_buf, &ctx->data);
                        ctx->data_fresh      = true;
                        ctx->state           = PMSX003_STATE_READY;
                        ctx->last_valid_tick = now;
                        ctx->error_count     = 0U;

#ifdef USART1_DEBUG_MODE
                        bsp_uart_transmit_string((uint8_t*)"[PMSX003] Frame OK\r\n");
#endif
                    }
                    else
                    {
                        ctx->state = PMSX003_STATE_ERROR;
                        ctx->error_count++;

#ifdef USART1_DEBUG_MODE
                        bsp_uart_transmit_string((uint8_t*)"[PMSX003] Checksum fail\r\n");
#endif
                    }

                    /* Reset for next frame */
                    ctx->rx_idx = 0U;
                }
            }
            else
            {
                /* Buffer overflow protection, reset */
                ctx->rx_idx = 0U;
                ctx->state  = PMSX003_STATE_IDLE;
            }
            break;
    }
}

status_e sensor_pmsx003_poll(pmsx003_ctx_s *ctx)
{
    uint32_t now;
    
    if (ctx == NULL)
    {
        return STATUS_ERR_PARAM;
    }

    now = bsp_systick_get_tick();

    /* Check for timeout during frame reception */
    if (ctx->state == PMSX003_STATE_RECEIVING)
    {
        if (bsp_systick_elapsed(ctx->last_rx_tick, PMSX003_RX_TIMEOUT_MS))
        {
            ctx->state  = PMSX003_STATE_ERROR;
            ctx->rx_idx = 0U;
            ctx->error_count++;

#ifdef USART1_DEBUG_MODE
            bsp_uart_transmit_string((uint8_t*)"[PMSX003] RX timeout\r\n");
#endif
            
            if (ctx->error_count >= PMSX003_MAX_ERRORS)
            {
                return STATUS_ERR_TIMEOUT;
            }
        }
    }

    /* Check for sensor disconnect: no valid frame for 5 seconds */
    if (bsp_systick_elapsed(ctx->last_valid_tick, 5000U))
    {
        /* Sensor disconnected or not responding */
        if (ctx->state == PMSX003_STATE_READY)
        {
            ctx->state = PMSX003_STATE_ERROR;
            ctx->data_fresh = false;
            ctx->error_count++;

#ifdef USART1_DEBUG_MODE
            bsp_uart_transmit_string((uint8_t*)"[PMSX003] Sensor disconnect detected\r\n");
#endif
        }
    }

    /* Refresh IWDG during polling */
    bsp_iwdg_refresh();

    return STATUS_OK;
}

const pmsx003_data_s* sensor_pmsx003_get_data(const pmsx003_ctx_s *ctx)
{
    if (ctx == NULL)
    {
        return NULL;
    }

    return &ctx->data;
}

bool sensor_pmsx003_is_data_fresh(const pmsx003_ctx_s *ctx)
{
    if (ctx == NULL)
    {
        return false;
    }

    return ctx->data_fresh;
}

void sensor_pmsx003_clear_fresh(pmsx003_ctx_s *ctx)
{
    if (ctx != NULL)
    {
        ctx->data_fresh = false;
    }
}

pmsx003_state_e sensor_pmsx003_get_state(const pmsx003_ctx_s *ctx)
{
    if (ctx == NULL)
    {
        return PMSX003_STATE_UNINIT;
    }

    return ctx->state;
}

status_e sensor_pmsx003_reset(pmsx003_ctx_s *ctx)
{
    if (ctx == NULL)
    {
        return STATUS_ERR_PARAM;
    }

    ctx->state       = PMSX003_STATE_IDLE;
    ctx->rx_idx      = 0U;
    ctx->data_fresh  = false;
    ctx->error_count = 0U;

#ifdef USART1_DEBUG_MODE
    bsp_uart_transmit_string((uint8_t*)"[PMSX003] Reset\r\n");
#endif

    return STATUS_OK;
}

/*
* @file infwin_co_sensor.c
* 
* Created on: 14 June 2026
*     Author: DST0x
*/

#include "drivers/sensor_infwin_co/infwin_co_sensor.h"

#ifdef USART1_MODBUS_MODE

static void request_command(uint8_t cmd[INFWIN_CO_REQUEST_LEN]){
    cmd[0U] = 0x62U;
    cmd[1U] = 0x03U;
    cmd[2U] = 0x00U;
    cmd[3U] = 0x03U;
    cmd[4U] = 0x00U;
    cmd[5U] = 0x01U;

    uint16_t crc = modbus_crc16(cmd, 6U);
    cmd[6U] = (uint8_t)(crc & 0x00FFU);
    cmd[7U] = (uint8_t)((crc >> 8U) & 0x00FFU);
}

static bool validate_response_crc(const uint8_t *buf){
    uint16_t crc_calc = modbus_crc16(buf, (uint16_t)(INFWIN_CO_RESPONSE_LEN - 2U));
    uint16_t crc_rx = (uint16_t)buf[INFWIN_CO_RESPONSE_LEN - 2U] | ((uint16_t)buf[INFWIN_CO_RESPONSE_LEN - 1U] << 8U);
    return (crc_calc == crc_rx);
}

static status_e handle_error(co_ctx_s *ctx, status_e status){
    if(ctx->error_count < INFWIN_CO_MAX_ERRORS){
        ctx->error_count++;
    }
    if(ctx->error_count >= INFWIN_CO_MAX_ERRORS){
        ctx->state = CO_STATE_ERROR;
    }
    return status;
}

static void apply_last_data_valid(co_ctx_s *ctx){
    if((ctx->data.co_value == 0U) && ctx->has_valid_data){
        ctx->data.co_value = ctx->last_valid_data.co_value;
    }
}

status_e sensor_co_init(co_ctx_s *ctx){
    if(ctx == NULL) {return STATUS_ERR_PARAM;}
    (void)memset(ctx, 0, sizeof(co_ctx_s));
    bsp_modbus_init();

    ctx->state      = CO_STATE_WARMING_UP;
    ctx->last_request_tick  = bsp_systick_get_tick();

    return STATUS_OK;
}

status_e sensor_co_send_request(co_ctx_s *ctx){
    uint8_t cmd[INFWIN_CO_REQUEST_LEN];
    if(ctx == NULL)                     {return STATUS_ERR_PARAM;}
    if(ctx->state == CO_STATE_UNINIT)   {return STATUS_ERR_STATE;}

    request_command(cmd);

    uint16_t sent = bsp_modbus_write(cmd, INFWIN_CO_REQUEST_LEN);
    if(sent != INFWIN_CO_REQUEST_LEN) { 
        return handle_error(ctx, STATUS_ERR_GENERIC);
    }
    ctx->waiting_response   = true;
    ctx->request_sent_tick  = bsp_systick_get_tick();
    ctx->rx_index           = 0U;

    ctx->data.request_count++;
    return STATUS_OK;
}

status_e sensor_co_poll(co_ctx_s *ctx){
    if(ctx == NULL){
        return STATUS_ERR_PARAM;
    }
    uint32_t now = bsp_systick_get_tick();
    if(ctx->state == CO_STATE_WARMING_UP){
        ctx->state = CO_STATE_RUNNING;
    }

    if (!ctx->waiting_response &&
        bsp_systick_elapsed(ctx->last_request_tick, INFWIN_CO_REQUEST_INTERVAL_MS)){
        ctx->last_request_tick = now;
        status_e status = sensor_co_send_request(ctx);
        if (status != STATUS_OK){
            return status;
        }
    }

    if (ctx->waiting_response && bsp_systick_elapsed(ctx->request_sent_tick,INFWIN_CO_RESPONSE_TIMEOUT_MS)){
            ctx->waiting_response = false;
            ctx->rx_index           = 0U;
            ctx->data.timeout_count++;
            return handle_error(ctx, STATUS_ERR_TIMEOUT);
    }

    
    if (ctx->waiting_response && (ctx->rx_index >= INFWIN_CO_RESPONSE_LEN)){
        ctx->waiting_response = false;

        if (!validate_response_crc(ctx->rx_buf)){
            ctx->rx_index = 0U;
            ctx->data.crc_error_count++;
            return handle_error(ctx, STATUS_ERR_CRC);
        }

        if ((ctx->rx_buf[0U] != INFWIN_CO_SENSOR_ADDR) || (ctx->rx_buf[1U] != 0x03U)){
            ctx->rx_index = 0U;
            return handle_error(ctx, STATUS_ERR_GENERIC);
        }

        ctx->data.response_count++;

        (void)memcpy(ctx->last_response, ctx->rx_buf, INFWIN_CO_RESPONSE_LEN);

        /* Parse Response */
        /* Option 1: Standard Big-Endian (MSB first) — DEFAULT */
        uint16_t co_raw = BYTES_TO_U16(ctx->rx_buf[3U], ctx->rx_buf[4U]);
        
        /* Option 2: Little-Endian (LSB first) */
        // uint16_t co_raw = BYTES_TO_U16(ctx->rx_buf[4U], ctx->rx_buf[3U]);
        
        /* Option 3: Scaled value (÷10) */
        // uint16_t co_raw = BYTES_TO_U16(ctx->rx_buf[3U], ctx->rx_buf[4U]);
        // co_raw = co_raw * 10U;  /* If sensor sends 0.1 ppm units */
        
        /* Option 4: Scaled value (×10) */
        // uint16_t co_raw = BYTES_TO_U16(ctx->rx_buf[3U], ctx->rx_buf[4U]);
        // co_raw = co_raw / 10U;  /* If sensor sends in 0.1 ppm resolution */
        
        ctx->data.co_value = co_raw;
        ctx->data.valid  = true;
        apply_last_data_valid(ctx);
        if (ctx->data.co_value != 0U){
            (void)memcpy(&ctx->last_valid_data, &ctx->data, sizeof(co_data_s));
            ctx->has_valid_data = true;
        }

        ctx->data_fresh  = true;
        ctx->error_count = 0U;
        ctx->rx_index    = 0U;

        return STATUS_OK;
    }

    return STATUS_NOT_READY;
}

co_state_e sensor_co_get_state(const co_ctx_s *ctx){
    return (ctx != NULL) ? ctx->state : CO_STATE_UNINIT;
}

const uint8_t* sensor_co_get_last_resp(const co_ctx_s *ctx){
    return (ctx != NULL) ? ctx->last_response : NULL;
}

void sensor_co_rx_byte(co_ctx_s *ctx, uint8_t byte){
    if(ctx == NULL){
        return;
    }
    if(!ctx->waiting_response){
        return;
    }
    if(ctx->rx_index < INFWIN_CO_RESPONSE_LEN){
        ctx->rx_buf[ctx->rx_index] = byte;
        ctx->rx_index++;
    }
}

#endif /* USART1_MODBUS_MODE */
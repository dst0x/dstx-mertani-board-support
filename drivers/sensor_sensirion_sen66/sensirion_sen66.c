/*
* @file sensirion_sen66.c
* 
* Created on: 14 June 2026
*     Author: DST0x
* 
* Modified: 15 July 2026
*     Integrated with sensor_manager for centralized control
*/

#include "drivers/sensor_sensirion_sen66/sensirion_sen66.h"
#include "bsp/bsp_iwdg.h"

#ifdef USART1_DEBUG_MODE
#include "bsp/bsp_uart.h"
#endif

static uint8_t calc_crc(const uint8_t data[2U])
{
    uint8_t crc = 0xFFU;
    uint8_t i, bit;
    for (i = 0U; i < 2U; i++)
    {
        crc ^= data[i];
        for (bit = 8U; bit > 0U; bit--)
        {
            crc = ((crc & 0x80U) != 0U) ? (uint8_t)((crc << 1U) ^ 0x31U) : (uint8_t)(crc << 1U);
        }
    }
    return crc;
}

uint8_t sensirion_sen66_calc_crc(const uint8_t data[2U]){
    return calc_crc(data);
}

static bool crc_ok(const uint8_t *buf){
    uint8_t pair[2U] = { buf[0U], buf[1U] };
    return (calc_crc(pair) == buf[2U]);
}

static status_e handle_err(sen66_ctx_s *ctx, status_e status){
    if(ctx->error_count < SEN66_MAX_ERRORS){
        ctx->error_count++;
    }
    if(ctx->error_count >= SEN66_MAX_ERRORS){
        ctx->state = SEN66_STATE_ERROR;
    }
    return status;
}

static bool is_unavail_u16(uint16_t val) {
    return (val == SEN66_UNAVAIL_U16);
}

static bool is_unavail_i16(int16_t val) {
    return (val == SEN66_UNAVAIL_I16);
}

static void parse_response_data(const uint8_t *buf, sen66_data_s *tmp){
    uint16_t pm1  = BYTES_TO_U16(buf[0U],  buf[1U]);
    uint16_t pm25 = BYTES_TO_U16(buf[3U],  buf[4U]);
    uint16_t pm40 = BYTES_TO_U16(buf[6U],  buf[7U]);
    uint16_t pm10 = BYTES_TO_U16(buf[9U],  buf[10U]);
    int16_t  rh   = BYTES_TO_I16(buf[12U], buf[13U]);
    int16_t  temp = BYTES_TO_I16(buf[15U], buf[16U]);
    int16_t  voc  = BYTES_TO_I16(buf[18U], buf[19U]);
    int16_t  nox  = BYTES_TO_I16(buf[21U], buf[22U]);
    uint16_t co2  = BYTES_TO_U16(buf[24U], buf[25U]);

#ifdef USART1_DEBUG_MODE
    /* Debug raw CO2 value */
    if (co2 == SEN66_UNAVAIL_U16) {
        bsp_debug_write_str("[SEN66] CO2 raw: UNAVAILABLE (0xFFFF)\r\n");
    } else {
        bsp_debug_write_str("[SEN66] CO2 raw: ");
        bsp_debug_write_int("", (int32_t)co2, " ppm\r\n");
    }
#endif

    tmp->pm_valid  = !is_unavail_u16(pm1);
    tmp->env_valid = !is_unavail_i16(rh);
    tmp->gas_valid = (!is_unavail_i16(voc)) && (!is_unavail_i16(nox));
    tmp->co2_valid = !is_unavail_u16(co2);

    tmp->pm1_0   = tmp->pm_valid  ? pm1  : 0U;
    tmp->pm2_5   = tmp->pm_valid  ? pm25 : 0U;
    tmp->pm4_0   = tmp->pm_valid  ? pm40 : 0U;
    tmp->pm10    = tmp->pm_valid  ? pm10 : 0U;
    tmp->co2_ppm = tmp->co2_valid ? co2  : 0U;
    
    tmp->rh      = tmp->env_valid ? (int16_t)(rh / (int16_t)10)   : (int16_t)0;
    tmp->voc     = tmp->gas_valid ? voc  : (int16_t)0;
    tmp->nox     = tmp->gas_valid ? nox  : (int16_t)0;
    
    tmp->temp    = tmp->env_valid ? (int16_t)(temp / (int16_t)20) : (int16_t)0;
}

static void apply_last_data_valid(sen66_data_s *tmp, const sen66_data_s *last){
    if ((!tmp->pm_valid) && (last->pm_valid)) { 
        tmp->pm1_0 = last->pm1_0; 
        tmp->pm2_5 = last->pm2_5; 
        tmp->pm4_0 = last->pm4_0; 
        tmp->pm10  = last->pm10;
        
        tmp->nc0_5 = last->nc0_5; 
        tmp->nc1_0 = last->nc1_0; 
        tmp->nc2_5 = last->nc2_5; 
        tmp->nc4_0 = last->nc4_0; 
        tmp->nc10  = last->nc10; 
        
        tmp->pm_valid = true; 
    }
    
    if ((!tmp->env_valid) && (last->env_valid)) { 
        tmp->rh   = last->rh;   
        tmp->temp = last->temp; 
        
        tmp->env_valid = true;
    }
    
    if ((!tmp->gas_valid) && (last->gas_valid)) { 
        tmp->voc = last->voc; 
        tmp->nox = last->nox; 
        
        tmp->gas_valid = true;
    }
    
    if ((!tmp->co2_valid) && (last->co2_valid)) { 
        tmp->co2_ppm = last->co2_ppm; 
        
        tmp->co2_valid = true;
    }
}

static void parse_nc_data(const uint8_t *buf, sen66_data_s *tmp){
    uint16_t nc0_5 = BYTES_TO_U16(buf[0U],  buf[1U]);
    uint16_t nc1_0 = BYTES_TO_U16(buf[3U],  buf[4U]);
    uint16_t nc2_5 = BYTES_TO_U16(buf[6U],  buf[7U]);
    uint16_t nc4_0 = BYTES_TO_U16(buf[9U],  buf[10U]);
    uint16_t nc10  = BYTES_TO_U16(buf[12U], buf[13U]);

    bool nc_valid = !is_unavail_u16(nc0_5);

    tmp->nc0_5 = nc_valid ? nc0_5 : 0U;
    tmp->nc1_0 = nc_valid ? nc1_0 : 0U;
    tmp->nc2_5 = nc_valid ? nc2_5 : 0U;
    tmp->nc4_0 = nc_valid ? nc4_0 : 0U;
    tmp->nc10  = nc_valid ? nc10  : 0U;

    tmp->typ_size = 0U;
}

static status_e verify_crc_words(const uint8_t *buf, uint8_t num_words){
    uint8_t i;
    for(i = 0U; i < num_words; i++){
        if(!crc_ok(&buf[(uint8_t)(i * 3U)])){
            return STATUS_ERR_CRC;
        }
    }
    return STATUS_OK;
}

status_e sensirion_sen66_init(sen66_ctx_s *ctx){
    status_e status;
    uint8_t prod_buf[48U];

    if(ctx == NULL){
        return STATUS_ERR_PARAM;
    }
    (void)memset(ctx, 0, sizeof(sen66_ctx_s));
    ctx->state = SEN66_STATE_UNINIT;

#ifdef USART1_DEBUG_MODE
    bsp_debug_write_str("[SEN66] Initializing sensor...\r\n");
#endif

    bsp_systick_delay_ms(SEN66_STARTUP_MS);
    status = bsp_i2c_write_data(SEN66_I2C_ADDR, SEN66_CMD_READ_PRODUCT, prod_buf, (uint8_t)sizeof(prod_buf));

    if(status != STATUS_OK){
#ifdef USART1_DEBUG_MODE
        bsp_debug_write_str("[SEN66] I2C communication failed\r\n");
#endif
        return handle_err(ctx, status);
    }
    if(verify_crc_words(prod_buf, 16U) != STATUS_OK){
#ifdef USART1_DEBUG_MODE
        bsp_debug_write_str("[SEN66] CRC verification failed\r\n");
#endif
        return handle_err(ctx, STATUS_ERR_CRC);
    }
    ctx->state          = SEN66_STATE_IDLE;
    ctx->error_count    = 0U;
    ctx->data_fresh     = false;
    
#ifdef USART1_DEBUG_MODE
    bsp_debug_write_str("[SEN66] Initialization successful\r\n");
#endif
    
    return STATUS_OK;
}

status_e sensirion_sen66_start_measurement(sen66_ctx_s *ctx){
    status_e status;
    if(ctx == NULL){
        return STATUS_ERR_PARAM;
    }

    if((ctx->state != SEN66_STATE_IDLE) && (ctx->state != SEN66_STATE_ERROR)){
        return STATUS_ERR_STATE;
    }

#ifdef USART1_DEBUG_MODE
    bsp_debug_write_str("[SEN66] Starting measurement...\r\n");
#endif

    status = bsp_i2c_write_cmd(SEN66_I2C_ADDR, SEN66_CMD_START_MEAS);
    if(status != STATUS_OK){
#ifdef USART1_DEBUG_MODE
        bsp_debug_write_str("[SEN66] Failed to start measurement\r\n");
#endif
        return handle_err(ctx, status);
    }

    bsp_systick_delay_ms(50U);

    ctx->state      = SEN66_STATE_WARMING_UP;
    ctx->start_tick = bsp_systick_get_tick();
    ctx->data_fresh = false;
    ctx->error_count= 0U;
    
#ifdef USART1_DEBUG_MODE
    bsp_debug_write_str("[SEN66] Measurement started, entering warmup phase\r\n");
#endif
    
    return STATUS_OK;
}

status_e sensirion_sen66_stop_measurement(sen66_ctx_s *ctx){
    status_e status;
    uint32_t elapsed, start_tick;
    
    if(ctx == NULL){
        return STATUS_ERR_PARAM;
    }
    status = bsp_i2c_write_cmd(SEN66_I2C_ADDR, SEN66_CMD_STOP_MEAS);
    
    start_tick = bsp_systick_get_tick();
    while (!bsp_systick_elapsed(start_tick, SEN66_STOP_DELAY_MS)) {
        elapsed = bsp_systick_get_tick() - start_tick;
        if ((elapsed % 500U) == 0U) {
            bsp_iwdg_refresh();
        }
    }
    
    ctx->state = SEN66_STATE_IDLE;
    return status;
}

status_e sensirion_sen66_poll(sen66_ctx_s *ctx){
    status_e status;
    uint8_t ready_buf[3U];
    uint8_t meas_buf[SEN66_MEAS_PAYLOAD_BYTES];
    uint8_t nc_buf[SEN66_NC_PAYLOAD_BYTES];

    if(ctx == NULL){
        return STATUS_ERR_PARAM;
    }
    if((ctx->state != SEN66_STATE_WARMING_UP) && (ctx->state != SEN66_STATE_RUNNING)){
        return STATUS_ERR_STATE;
    }

    if((ctx->state == SEN66_STATE_WARMING_UP) && bsp_systick_elapsed(ctx->start_tick, SEN66_WARMUP_MS)){
        ctx->state = SEN66_STATE_RUNNING;
    }

    status = bsp_i2c_write_data(SEN66_I2C_ADDR, SEN66_CMD_DATA_READY, ready_buf, 3U);

    if (status != STATUS_OK){
        return handle_err(ctx, status);
    }
    if (!crc_ok(ready_buf)){
        return handle_err(ctx, STATUS_ERR_CRC); 
    }
    if (ready_buf[1U] != 0x01U){
        return STATUS_NOT_READY;
    }

    status = bsp_i2c_write_data(SEN66_I2C_ADDR, SEN66_CMD_READ_VALUES, meas_buf, SEN66_MEAS_PAYLOAD_BYTES);

    if (status != STATUS_OK){
        return handle_err(ctx, status);
    }
    if (verify_crc_words(meas_buf, 9U) != STATUS_OK){
        return handle_err(ctx, STATUS_ERR_CRC);
    }
    (void)memcpy(ctx->raw_meas, meas_buf, SEN66_MEAS_PAYLOAD_BYTES);

    status = bsp_i2c_write_data(SEN66_I2C_ADDR, SEN66_CMD_READ_PM_NC, nc_buf, SEN66_NC_PAYLOAD_BYTES);
    if (status != STATUS_OK) { 
        return handle_err(ctx, status); 
    }
    if (verify_crc_words(nc_buf, 5U) != STATUS_OK) { 
        return handle_err(ctx, STATUS_ERR_CRC); 
    }

    parse_response_data(meas_buf, &ctx->data);
    parse_nc_data(nc_buf, &ctx->data);

    if (ctx->has_valid_data){
        apply_last_data_valid(&ctx->data, &ctx->last_valid_data);
    }

    bool has_nonzero_data = false;
    if (ctx->data.pm_valid && (ctx->data.pm1_0 != 0U || ctx->data.pm2_5 != 0U || ctx->data.pm4_0 != 0U || ctx->data.pm10 != 0U)) {
        has_nonzero_data = true;
    }
    if (ctx->data.env_valid && (ctx->data.rh != 0 || ctx->data.temp != 0)) {
        has_nonzero_data = true;
    }
    if (ctx->data.gas_valid && (ctx->data.voc != 0 || ctx->data.nox != 0)) {
        has_nonzero_data = true;
    }
    if (ctx->data.co2_valid && (ctx->data.co2_ppm != 0U)) {
        has_nonzero_data = true;
    }
    
    if (has_nonzero_data)
    {
        (void)memcpy(&ctx->last_valid_data, &ctx->data, sizeof(sen66_data_s));
        ctx->has_valid_data = true;
    }

    ctx->data_fresh     = true;
    ctx->last_read_tick = bsp_systick_get_tick();
    ctx->error_count    = 0U;
    return STATUS_OK;
}

status_e sensirion_sen66_reset(sen66_ctx_s *ctx)
{
    status_e status;
    uint32_t elapsed, start_tick;
    
    if (ctx == NULL){
        return STATUS_ERR_PARAM; 
    }
    
#ifdef USART1_DEBUG_MODE
    bsp_debug_write_str("[SEN66] Resetting sensor...\r\n");
#endif
    
    (void)bsp_i2c_write_cmd(SEN66_I2C_ADDR, SEN66_CMD_RESET);
    
    start_tick = bsp_systick_get_tick();
    while (!bsp_systick_elapsed(start_tick, SEN66_RESET_DELAY_MS)) {
        elapsed = bsp_systick_get_tick() - start_tick;
        if ((elapsed % 500U) == 0U) {
            bsp_iwdg_refresh();
        }
    }
    
    status = sensirion_sen66_init(ctx);
    if (status != STATUS_OK){
        ctx->state       = SEN66_STATE_ERROR;
        ctx->error_count = SEN66_MAX_ERRORS;
#ifdef USART1_DEBUG_MODE
        bsp_debug_write_str("[SEN66] Reset failed\r\n");
#endif
    } else {
#ifdef USART1_DEBUG_MODE
        bsp_debug_write_str("[SEN66] Reset successful\r\n");
#endif
    }
    return status;
}

sen66_state_e sensirion_sen66_get_state(const sen66_ctx_s *ctx)
{
    return (ctx != NULL) ? ctx->state : SEN66_STATE_UNINIT;
}

bool sensirion_sen66_is_data_ready(sen66_ctx_s *ctx)
{
    uint8_t  ready_buf[3U];
    status_e status;

    if (ctx == NULL) { return false; }
    if ((ctx->state != SEN66_STATE_WARMING_UP) && (ctx->state != SEN66_STATE_RUNNING)){
        return false;
    }

    status = bsp_i2c_write_data(SEN66_I2C_ADDR, SEN66_CMD_DATA_READY, ready_buf, 3U);
    if (status != STATUS_OK) { 
        return false; 
    }
    if (!crc_ok(ready_buf)) { 
        return false; 
    }
    return (ready_buf[1U] == 0x01U);
}

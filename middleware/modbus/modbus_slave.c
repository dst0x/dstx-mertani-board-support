/*
* @file modbus_slave.h
* 
* Created on: 14 June 2026
*     Author: DST0x
*/

#include "middleware/modbus/modbus_slave.h"

#ifdef USART1_MODBUS_MODE
    static uint16_t last_valid_co_value = 0U;
#endif

#ifdef USART1_MODBUS_MODE
    static void build_register_map(const sen66_ctx_s *aq, const co_ctx_s *co, uint16_t *regs)
#else
    static void build_register_map(const sen66_ctx_s *aq, uint16_t *regs)
#endif
{
    const sen66_data_s *d = &aq->data;
    regs[0x00U] = d->pm1_0;
    regs[0x01U] = d->pm2_5;
    regs[0x02U] = d->pm4_0;
    regs[0x03U] = d->pm10;
    regs[0x04U] = (uint16_t)d->rh;
    regs[0x05U] = (uint16_t)d->temp;
    regs[0x06U] = (uint16_t)d->voc;
    regs[0x07U] = (uint16_t)d->nox;
    regs[0x08U] = d->co2_ppm;
    regs[0x09U] = d->nc0_5;
    regs[0x0AU] = d->nc1_0;
    regs[0x0BU] = d->nc2_5;
    regs[0x0CU] = d->nc4_0;
    regs[0x0DU] = d->nc10;
    regs[0x0EU] = (uint16_t)(
        ((d->pm_valid  ? 1U : 0U) << 0U) |
        ((d->env_valid ? 1U : 0U) << 1U) |
        ((d->gas_valid ? 1U : 0U) << 2U) |
        ((d->co2_valid ? 1U : 0U) << 3U)
    );
    regs[0x0FU] = (uint16_t)aq->state;
    regs[0x10U] = (uint16_t)aq->error_count;

#ifdef USART1_MODBUS_MODE
    if (co != NULL){
        if (co->data.co_value != 0U){
            last_valid_co_value = co->data.co_value;
        }
        regs[0x11U] = last_valid_co_value;
        regs[0x12U] = (uint16_t)co->data.request_count;
        regs[0x13U] = (uint16_t)co->data.response_count;
        regs[0x14U] = (uint16_t)co->data.timeout_count;
        regs[0x15U] = (uint16_t)co->data.crc_error_count;
    }else{
        regs[0x11U] = 0U;
        regs[0x12U] = 0U;
        regs[0x13U] = 0U;
        regs[0x14U] = 0U;
        regs[0x15U] = 0U;
    }
#else
    regs[0x11U] = 0U;
    regs[0x12U] = 0U;
    regs[0x13U] = 0U;
    regs[0x14U] = 0U;
    regs[0x15U] = 0U;
#endif
}

static uint16_t append_crc(uint8_t *buf, uint16_t len){
    uint16_t crc = modbus_crc16(buf, len);
    buf[len]        = (uint8_t)(crc & 0x00FFU);
    buf[len + 1U]   = (uint8_t)((crc >> 8U) & 0x00FFU);
    return len + 2U;        
}

static uint16_t build_exception(uint8_t *buf, uint8_t fc, uint8_t ex){
    buf[0U] = MB_SLAVE_ID;
    buf[1U] = fc | 0x80U;
    buf[2U] = ex;
    return append_crc(buf, 3U);
}

void modbus_slave_init(modbus_slave_ctx_s *ctx){
    if (ctx == NULL) { return; }
    (void)memset(ctx, 0, sizeof(modbus_slave_ctx_s));
}

void modbus_slave_rx_byte(modbus_slave_ctx_s *ctx, uint8_t byte, uint32_t tick){
    if (ctx == NULL) { return; }
    if ((ctx->rx_len > 0U) && bsp_systick_elapsed(ctx->last_rx_tick, MB_FRAME_TIMEOUT_MS)){
        ctx->rx_len      = 0U;
        ctx->frame_ready = false;
    }
    if (ctx->frame_ready) { return; }
    if (ctx->rx_len < MB_RX_BUF_SIZE){
        ctx->rx_buf[ctx->rx_len] = byte;
        ctx->rx_len++;
    }
    ctx->last_rx_tick = tick;
}

#ifdef USART1_MODBUS_MODE
    void modbus_slave_process(modbus_slave_ctx_s *ctx, const sen66_ctx_s *sensor_ctx, const co_ctx_s *co_ctx, uint16_t *tx_len)
#else
    void modbus_slave_process(modbus_slave_ctx_s *ctx, const sen66_ctx_s *sensor_ctx, uint16_t *tx_len)
#endif
{
    uint16_t regs[MB_REG_COUNT];
    uint16_t crc_rx, crc_calc;
    uint8_t  fc;
    uint16_t start_addr, qty, i, byte_cnt;

    if ((ctx == NULL) || (sensor_ctx == NULL) || (tx_len == NULL)) {
        return; 
    }
    *tx_len = 0U;

    if (ctx->rx_len < 6U) { goto done; }
    if (ctx->rx_buf[0U] != MB_SLAVE_ID) { goto done; }

    crc_calc = modbus_crc16(ctx->rx_buf, (uint16_t)(ctx->rx_len - 2U));
    crc_rx   = (uint16_t)ctx->rx_buf[ctx->rx_len - 2U] |
               ((uint16_t)ctx->rx_buf[ctx->rx_len - 1U] << 8U);
    if (crc_calc != crc_rx) { goto done; }

    fc = ctx->rx_buf[1U];
    if ((fc != MB_FC_READ_HOLDING) && (fc != MB_FC_READ_INPUT))
    {
        *tx_len = build_exception(ctx->tx_buf, fc, MB_EX_ILLEGAL_FUNC);
        goto done;
    }

    start_addr = (uint16_t)((uint16_t)ctx->rx_buf[2U] << 8U) | (uint16_t)ctx->rx_buf[3U];
    qty        = (uint16_t)((uint16_t)ctx->rx_buf[4U] << 8U) | (uint16_t)ctx->rx_buf[5U];

    if ((qty == 0U) || (qty > (uint16_t)MB_REG_COUNT))
    {
        *tx_len = build_exception(ctx->tx_buf, fc, MB_EX_ILLEGAL_VALUE);
        goto done;
    }
    if ((start_addr > (uint16_t)MB_REG_LAST) ||
        ((start_addr + qty - 1U) > (uint16_t)MB_REG_LAST))
    {
        *tx_len = build_exception(ctx->tx_buf, fc, MB_EX_ILLEGAL_ADDR);
        goto done;
    }

#ifdef USART1_MODBUS_MODE
    build_register_map(sensor_ctx, co_ctx, regs);
#else
    build_register_map(sensor_ctx, regs);
#endif
    byte_cnt = (uint16_t)(qty * 2U);
    ctx->tx_buf[0U] = MB_SLAVE_ID;
    ctx->tx_buf[1U] = fc;
    ctx->tx_buf[2U] = (uint8_t)(byte_cnt & 0x00FFU);
    for (i = 0U; i < qty; i++){
        uint16_t reg_val = regs[start_addr + i];
        ctx->tx_buf[3U + (i * 2U)]      = (uint8_t)((reg_val >> 8U) & 0x00FFU);
        ctx->tx_buf[3U + (i * 2U) + 1U] = (uint8_t)(reg_val & 0x00FFU);
    }
    *tx_len = append_crc(ctx->tx_buf, (uint16_t)(3U + byte_cnt));
done:
    ctx->rx_len      = 0U;
    ctx->frame_ready = false;
}

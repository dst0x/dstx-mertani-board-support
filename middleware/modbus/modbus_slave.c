/*
* @file modbus_slave.c
*
* Created on: 14 June 2026
*     Author: DST0x
*/

#include "middleware/modbus/modbus_slave.h"
#include "bsp/bsp_flash.h"
#include "bsp/bsp_uart.h"

static uint8_t  g_slave_id  = MB_SLAVE_ID;
static uint8_t  g_baud_code = MB_BAUD_CODE_9600;
static uint8_t  g_parity    = MB_PARITY_NONE;
static uint8_t  g_stopbits  = MB_STOPBITS_1;

#ifdef USART1_MODBUS_MODE
    static uint16_t last_valid_co_value = 0U;
#endif

static uint32_t baud_code_to_bps(uint8_t code)
{
    switch (code) {
        case MB_BAUD_CODE_1200:   return 1200U;
        case MB_BAUD_CODE_2400:   return 2400U;
        case MB_BAUD_CODE_4800:   return 4800U;
        case MB_BAUD_CODE_9600:   return 9600U;
        case MB_BAUD_CODE_19200:  return 19200U;
        case MB_BAUD_CODE_38400:  return 38400U;
        case MB_BAUD_CODE_57600:  return 57600U;
        case MB_BAUD_CODE_115200: return 115200U;
        default:                  return 9600U;
    }
}

static uint8_t bps_to_baud_code(uint32_t bps)
{
    switch (bps) {
        case 1200U:   return MB_BAUD_CODE_1200;
        case 2400U:   return MB_BAUD_CODE_2400;
        case 4800U:   return MB_BAUD_CODE_4800;
        case 9600U:   return MB_BAUD_CODE_9600;
        case 19200U:  return MB_BAUD_CODE_19200;
        case 38400U:  return MB_BAUD_CODE_38400;
        case 57600U:  return MB_BAUD_CODE_57600;
        case 115200U: return MB_BAUD_CODE_115200;
        default:      return MB_BAUD_CODE_9600;
    }
}

static uint32_t cfg_read_payload(uint32_t offset)
{
    uint32_t magic = bsp_flash_read_word(BSP_FLASH_CFG_PAGE_ADDR + offset);
    if (magic == BSP_FLASH_CFG_MAGIC) {
        return bsp_flash_read_word(BSP_FLASH_CFG_PAGE_ADDR + offset + 4U);
    }
    return 0xFFFFFFFFUL;
}

static void config_load(void)
{
    uint32_t val;

    val = cfg_read_payload(BSP_FLASH_CFG_OFF_SLAVE_ID);
    if ((val != 0xFFFFFFFFUL) &&
        (val >= (uint32_t)MB_SLAVE_ID_MIN) &&
        (val <= (uint32_t)MB_SLAVE_ID_MAX)) {
        g_slave_id = (uint8_t)val;
    }

    val = cfg_read_payload(BSP_FLASH_CFG_OFF_BAUDRATE);
    if (val != 0xFFFFFFFFUL) {
        uint8_t code = bps_to_baud_code(val);
        if ((code >= MB_BAUD_CODE_MIN) && (code <= MB_BAUD_CODE_MAX)) {
            g_baud_code = code;
        }
    }

    val = cfg_read_payload(BSP_FLASH_CFG_OFF_PARITY);
    if ((val != 0xFFFFFFFFUL) && (val <= (uint32_t)MB_PARITY_ODD)) {
        g_parity = (uint8_t)val;
    }

    val = cfg_read_payload(BSP_FLASH_CFG_OFF_STOPBITS);
    if ((val == (uint32_t)MB_STOPBITS_1) || (val == (uint32_t)MB_STOPBITS_2)) {
        g_stopbits = (uint8_t)val;
    }
}

static void config_save(void)
{
    (void)bsp_flash_write_config(g_slave_id,
                                 baud_code_to_bps(g_baud_code),
                                 g_parity,
                                 g_stopbits);
}

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

#ifdef USART1_MODBUS_MODE
    if (co != NULL) {
        if (co->data.co_value != 0U) {
            last_valid_co_value = co->data.co_value;
        }
        regs[0x09U] = last_valid_co_value;
    } else {
        regs[0x09U] = 0U;
    }
#else
    regs[0x09U] = 0U;
#endif

    regs[0x64U] = (uint16_t)aq->state;
    regs[0x65U] = (uint16_t)aq->error_count;
    regs[0x66U] = (uint16_t)(
        ((d->pm_valid  ? 1U : 0U) << 0U) |
        ((d->env_valid ? 1U : 0U) << 1U) |
        ((d->gas_valid ? 1U : 0U) << 2U) |
        ((d->co2_valid ? 1U : 0U) << 3U)
    );

#ifdef USART1_MODBUS_MODE
    if (co != NULL) {
        regs[0x67U] = (uint16_t)co->state;
        regs[0x68U] = (uint16_t)co->data.request_count;
        regs[0x69U] = (uint16_t)co->data.response_count;
        regs[0x6AU] = (uint16_t)co->data.timeout_count;
        regs[0x6BU] = (uint16_t)co->data.crc_error_count;
    } else {
        regs[0x67U] = 0xFFFFU;
        regs[0x68U] = 0U;
        regs[0x69U] = 0U;
        regs[0x6AU] = 0U;
        regs[0x6BU] = 0U;
    }
#else
    regs[0x67U] = 0U;
    regs[0x68U] = 0U;
    regs[0x69U] = 0U;
    regs[0x6AU] = 0U;
    regs[0x6BU] = 0U;
#endif

    regs[MB_REG_CFG_SLAVE_ID]  = (uint16_t)g_slave_id;
    regs[MB_REG_CFG_BAUDRATE]  = (uint16_t)g_baud_code;
    regs[MB_REG_CFG_PARITY]    = (uint16_t)g_parity;
    regs[MB_REG_CFG_STOPBITS]  = (uint16_t)g_stopbits;
    
    /* Last update timestamp in seconds (tick / 1000) */
    uint32_t last_update_sec = (uint32_t)(aq->last_read_tick / 1000U);
    regs[MB_REG_CFG_LAST_UPDATE] = (uint16_t)(last_update_sec & 0xFFFFU);
}

static uint16_t append_crc(uint8_t *buf, uint16_t len)
{
    uint16_t crc = modbus_crc16(buf, len);
    buf[len]      = (uint8_t)(crc & 0x00FFU);
    buf[len + 1U] = (uint8_t)((crc >> 8U) & 0x00FFU);
    return len + 2U;
}

static uint16_t build_exception(uint8_t *buf, uint8_t fc, uint8_t ex)
{
    buf[0U] = g_slave_id;
    buf[1U] = fc | 0x80U;
    buf[2U] = ex;
    return append_crc(buf, 3U);
}

void modbus_slave_init(modbus_slave_ctx_s *ctx)
{
    if (ctx == NULL) { return; }
    (void)memset(ctx, 0, sizeof(modbus_slave_ctx_s));
    config_load();
    (void)bsp_rs485_reinit(baud_code_to_bps(g_baud_code), g_parity, g_stopbits);
}

void modbus_slave_rx_byte(modbus_slave_ctx_s *ctx, uint8_t byte, uint32_t tick)
{
    if (ctx == NULL) { return; }
    if ((ctx->rx_len > 0U) && bsp_systick_elapsed(ctx->last_rx_tick, MB_FRAME_TIMEOUT_MS)) {
        ctx->rx_len      = 0U;
        ctx->frame_ready = false;
    }
    if (ctx->frame_ready) { return; }
    if (ctx->rx_len < MB_RX_BUF_SIZE) {
        ctx->rx_buf[ctx->rx_len] = byte;
        ctx->rx_len++;
    }
    ctx->last_rx_tick = tick;
}

#ifdef USART1_MODBUS_MODE
    void modbus_slave_process(modbus_slave_ctx_s *ctx, const sen66_ctx_s *sensor_ctx,
                              const co_ctx_s *co_ctx, uint16_t *tx_len)
#else
    void modbus_slave_process(modbus_slave_ctx_s *ctx, const sen66_ctx_s *sensor_ctx,
                              uint16_t *tx_len)
#endif
{
    uint16_t regs[MB_REG_COUNT];
    uint16_t crc_rx, crc_calc;
    uint8_t  fc;
    uint16_t start_addr, qty, i, byte_cnt;

    if ((ctx == NULL) || (sensor_ctx == NULL) || (tx_len == NULL)) { return; }
    *tx_len = 0U;

    if (ctx->rx_len < 6U) { goto done; }
    if (ctx->rx_buf[0U] != g_slave_id) { goto done; }

    crc_calc = modbus_crc16(ctx->rx_buf, (uint16_t)(ctx->rx_len - 2U));
    crc_rx   = (uint16_t)ctx->rx_buf[ctx->rx_len - 2U] |
               ((uint16_t)ctx->rx_buf[ctx->rx_len - 1U] << 8U);
    if (crc_calc != crc_rx) { goto done; }

    fc = ctx->rx_buf[1U];

    if (fc == MB_FC_WRITE_SINGLE) {
        uint16_t wr_addr, wr_val;
        bool cfg_changed = false;

        if (ctx->rx_len < 8U) { goto done; }

        wr_addr = (uint16_t)((uint16_t)ctx->rx_buf[2U] << 8U) | (uint16_t)ctx->rx_buf[3U];
        wr_val  = (uint16_t)((uint16_t)ctx->rx_buf[4U] << 8U) | (uint16_t)ctx->rx_buf[5U];

        switch (wr_addr) {
            case MB_REG_CFG_SLAVE_ID:
                if ((wr_val < (uint16_t)MB_SLAVE_ID_MIN) ||
                    (wr_val > (uint16_t)MB_SLAVE_ID_MAX)) {
                    *tx_len = build_exception(ctx->tx_buf, fc, MB_EX_ILLEGAL_VALUE);
                    goto done;
                }
                g_slave_id  = (uint8_t)wr_val;
                cfg_changed = true;
                break;

            case MB_REG_CFG_BAUDRATE:
                if ((wr_val < (uint16_t)MB_BAUD_CODE_MIN) ||
                    (wr_val > (uint16_t)MB_BAUD_CODE_MAX)) {
                    *tx_len = build_exception(ctx->tx_buf, fc, MB_EX_ILLEGAL_VALUE);
                    goto done;
                }
                g_baud_code = (uint8_t)wr_val;
                cfg_changed = true;
                break;

            case MB_REG_CFG_PARITY:
                if (wr_val > (uint16_t)MB_PARITY_ODD) {
                    *tx_len = build_exception(ctx->tx_buf, fc, MB_EX_ILLEGAL_VALUE);
                    goto done;
                }
                g_parity    = (uint8_t)wr_val;
                cfg_changed = true;
                break;

            case MB_REG_CFG_STOPBITS:
                if ((wr_val != (uint16_t)MB_STOPBITS_1) &&
                    (wr_val != (uint16_t)MB_STOPBITS_2)) {
                    *tx_len = build_exception(ctx->tx_buf, fc, MB_EX_ILLEGAL_VALUE);
                    goto done;
                }
                g_stopbits  = (uint8_t)wr_val;
                cfg_changed = true;
                break;

            case MB_REG_CFG_LAST_UPDATE:
                /* Read-only register, reject write */
                *tx_len = build_exception(ctx->tx_buf, fc, MB_EX_ILLEGAL_ADDR);
                goto done;

            default:
                *tx_len = build_exception(ctx->tx_buf, fc, MB_EX_ILLEGAL_ADDR);
                goto done;
        }

        if (cfg_changed) {
            /* Echo response at current baudrate before switching */
            ctx->tx_buf[0U] = g_slave_id;
            ctx->tx_buf[1U] = MB_FC_WRITE_SINGLE;
            ctx->tx_buf[2U] = ctx->rx_buf[2U];
            ctx->tx_buf[3U] = ctx->rx_buf[3U];
            ctx->tx_buf[4U] = ctx->rx_buf[4U];
            ctx->tx_buf[5U] = ctx->rx_buf[5U];
            *tx_len = append_crc(ctx->tx_buf, 6U);

            config_save();
            (void)bsp_rs485_reinit(baud_code_to_bps(g_baud_code), g_parity, g_stopbits);
        }
        goto done;
    }

    if ((fc != MB_FC_READ_HOLDING) && (fc != MB_FC_READ_INPUT)) {
        *tx_len = build_exception(ctx->tx_buf, fc, MB_EX_ILLEGAL_FUNC);
        goto done;
    }

    start_addr = (uint16_t)((uint16_t)ctx->rx_buf[2U] << 8U) | (uint16_t)ctx->rx_buf[3U];
    qty        = (uint16_t)((uint16_t)ctx->rx_buf[4U] << 8U) | (uint16_t)ctx->rx_buf[5U];

    if ((qty == 0U) || (qty > (uint16_t)MB_REG_COUNT)) {
        *tx_len = build_exception(ctx->tx_buf, fc, MB_EX_ILLEGAL_VALUE);
        goto done;
    }
    if ((start_addr > (uint16_t)MB_REG_LAST) ||
        ((start_addr + qty - 1U) > (uint16_t)MB_REG_LAST)) {
        *tx_len = build_exception(ctx->tx_buf, fc, MB_EX_ILLEGAL_ADDR);
        goto done;
    }

    (void)memset(regs, 0, sizeof(regs));

#ifdef USART1_MODBUS_MODE
    build_register_map(sensor_ctx, co_ctx, regs);
#else
    build_register_map(sensor_ctx, regs);
#endif

    byte_cnt        = (uint16_t)(qty * 2U);
    ctx->tx_buf[0U] = g_slave_id;
    ctx->tx_buf[1U] = fc;
    ctx->tx_buf[2U] = (uint8_t)(byte_cnt & 0x00FFU);
    for (i = 0U; i < qty; i++) {
        uint16_t reg_val = regs[start_addr + i];
        ctx->tx_buf[3U + (i * 2U)]      = (uint8_t)((reg_val >> 8U) & 0x00FFU);
        ctx->tx_buf[3U + (i * 2U) + 1U] = (uint8_t)(reg_val & 0x00FFU);
    }
    *tx_len = append_crc(ctx->tx_buf, (uint16_t)(3U + byte_cnt));

done:
    ctx->rx_len      = 0U;
    ctx->frame_ready = false;
}

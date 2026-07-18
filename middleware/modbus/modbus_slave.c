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

/* PMSX003 calibration factors (fixed-point ×1000, default based on SEN66 correlation) */
#ifdef USART1_SENSOR_MODE
static uint16_t g_pmsx003_cal_pm1_0 = 160U;   /* 0.16x - empirically matched to SEN66 */
static uint16_t g_pmsx003_cal_pm2_5 = 165U;   /* 0.165x - empirically matched to SEN66 */
static uint16_t g_pmsx003_cal_pm10  = 180U;   /* 0.18x - empirically matched to SEN66 */
#define PMSX003_CAL_MIN (100U)   /* 0.1x */
#define PMSX003_CAL_MAX (10000U) /* 10.0x */
#endif

static uint16_t last_valid_sen66_regs[9] = {0U};
static uint8_t  sen66_zero_poll_count = 0U;
static bool     sen66_has_last_valid = false;

#ifdef USART1_MODBUS_MODE
    static uint16_t last_valid_co_value = 0U;
    static uint8_t  co_zero_poll_count = 0U;
    static bool     co_has_last_valid = false;
#endif

#ifdef USART1_SENSOR_MODE
    static uint16_t last_valid_pmsx003_pm1_high = 0U;
    static uint16_t last_valid_pmsx003_pm1_low  = 0U;
    static uint16_t last_valid_pmsx003_pm2_5_high = 0U;
    static uint16_t last_valid_pmsx003_pm2_5_low  = 0U;
    static uint16_t last_valid_pmsx003_pm10_high = 0U;
    static uint16_t last_valid_pmsx003_pm10_low  = 0U;
    static uint8_t  pmsx003_zero_poll_count = 0U;  /* Track consecutive polls without update */
    static bool     pmsx003_has_last_valid = false;
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

#ifdef USART1_SENSOR_MODE
    /* Load PMSX003 calibration factors */
    val = cfg_read_payload(BSP_FLASH_CFG_OFF_PMSX003_PM1_0);
    if ((val != 0xFFFFFFFFUL) && (val >= PMSX003_CAL_MIN) && (val <= PMSX003_CAL_MAX)) {
        g_pmsx003_cal_pm1_0 = (uint16_t)val;
    }

    val = cfg_read_payload(BSP_FLASH_CFG_OFF_PMSX003_PM2_5);
    if ((val != 0xFFFFFFFFUL) && (val >= PMSX003_CAL_MIN) && (val <= PMSX003_CAL_MAX)) {
        g_pmsx003_cal_pm2_5 = (uint16_t)val;
    }

    val = cfg_read_payload(BSP_FLASH_CFG_OFF_PMSX003_PM10);
    if ((val != 0xFFFFFFFFUL) && (val >= PMSX003_CAL_MIN) && (val <= PMSX003_CAL_MAX)) {
        g_pmsx003_cal_pm10 = (uint16_t)val;
    }
#endif
}

static void config_save(void)
{
#ifdef USART1_SENSOR_MODE
    (void)bsp_flash_write_config(g_slave_id,
                                 baud_code_to_bps(g_baud_code),
                                 g_parity,
                                 g_stopbits,
                                 g_pmsx003_cal_pm1_0,
                                 g_pmsx003_cal_pm2_5,
                                 g_pmsx003_cal_pm10);
#else
    (void)bsp_flash_write_config(g_slave_id,
                                 baud_code_to_bps(g_baud_code),
                                 g_parity,
                                 g_stopbits,
                                 160U, 165U, 180U);  /* Default calibration matched to SEN66 */
#endif
}

#if defined(USART1_MODBUS_MODE) && defined(USART1_SENSOR_MODE)
    static void build_register_map(const sen66_ctx_s *aq, const co_ctx_s *co, const pmsx003_ctx_s *pmsx003, uint16_t *regs)
#elif defined(USART1_MODBUS_MODE)
    static void build_register_map(const sen66_ctx_s *aq, const co_ctx_s *co, uint16_t *regs)
#elif defined(USART1_SENSOR_MODE)
    static void build_register_map(const sen66_ctx_s *aq, const pmsx003_ctx_s *pmsx003, uint16_t *regs)
#else
    static void build_register_map(const sen66_ctx_s *aq, uint16_t *regs)
#endif
{
    const sen66_data_s *d = &aq->data;
    uint8_t i;

    if (aq->has_valid_data) {
        regs[0x00U] = d->pm1_0;
        regs[0x01U] = d->pm2_5;
        regs[0x02U] = d->pm4_0;
        regs[0x03U] = d->pm10;
        regs[0x04U] = (uint16_t)d->rh;
        regs[0x05U] = (uint16_t)d->temp;
        regs[0x06U] = (uint16_t)d->voc;
        regs[0x07U] = (uint16_t)d->nox;
        regs[0x08U] = d->co2_ppm;

        for (i = 0U; i < 9U; i++) {
            last_valid_sen66_regs[i] = regs[i];
        }
        sen66_zero_poll_count = 0U;
        sen66_has_last_valid = true;
    } else if (sen66_has_last_valid && (sen66_zero_poll_count < 14U)) {
        for (i = 0U; i < 9U; i++) {
            regs[i] = last_valid_sen66_regs[i];
        }
        sen66_zero_poll_count++;
    } else {
        for (i = 0U; i < 9U; i++) {
            regs[i] = 0U;
        }
        if (sen66_zero_poll_count < 15U) {
            sen66_zero_poll_count = 15U;
        }
    }

#ifdef USART1_MODBUS_MODE
    if (co != NULL) {
        if (co->has_valid_data) {
            last_valid_co_value = co->data.co_value;
            co_zero_poll_count = 0U;
            co_has_last_valid = true;
            regs[0x09U] = co->data.co_value;
        } else if (co_has_last_valid && (co_zero_poll_count < 14U)) {
            regs[0x09U] = last_valid_co_value;
            co_zero_poll_count++;
        } else {
            regs[0x09U] = 0U;
            if (co_zero_poll_count < 15U) {
                co_zero_poll_count = 15U;
            }
        }
    } else {
        regs[0x09U] = 0U;
    }
#else
    regs[0x09U] = 0U;
#endif

    /* PMSX003 data registers (0x0410-0x0415) - IEEE 754 float DCBA byte order (little-endian) */
#ifdef USART1_SENSOR_MODE
    if (pmsx003 != NULL && pmsx003->state == PMSX003_STATE_READY) {
        const pmsx003_data_s *pm = &pmsx003->data;
        
        /* Apply calibration factors (fixed-point ×1000) and convert to float */
        /* PM1.0 - DCBA byte order */
        float pm1_0_float = ((float)pm->pm1_0_ug_m3 * (float)g_pmsx003_cal_pm1_0) / 1000.0f;
        uint32_t pm1_0_bits;
        (void)memcpy(&pm1_0_bits, &pm1_0_float, sizeof(uint32_t));
        uint16_t pm1_high = (uint16_t)(pm1_0_bits & 0xFFFFU);        /* Low 16 bits (BA) */
        uint16_t pm1_low  = (uint16_t)((pm1_0_bits >> 16U) & 0xFFFFU); /* High 16 bits (DC) */
        
        /* PM2.5 - DCBA byte order */
        float pm2_5_float = ((float)pm->pm2_5_ug_m3 * (float)g_pmsx003_cal_pm2_5) / 1000.0f;
        uint32_t pm2_5_bits;
        (void)memcpy(&pm2_5_bits, &pm2_5_float, sizeof(uint32_t));
        uint16_t pm2_5_high = (uint16_t)(pm2_5_bits & 0xFFFFU);        /* Low 16 bits (BA) */
        uint16_t pm2_5_low  = (uint16_t)((pm2_5_bits >> 16U) & 0xFFFFU); /* High 16 bits (DC) */
        
        /* PM10 - DCBA byte order */
        float pm10_float = ((float)pm->pm10_ug_m3 * (float)g_pmsx003_cal_pm10) / 1000.0f;
        uint32_t pm10_bits;
        (void)memcpy(&pm10_bits, &pm10_float, sizeof(uint32_t));
        uint16_t pm10_high = (uint16_t)(pm10_bits & 0xFFFFU);        /* Low 16 bits (BA) */
        uint16_t pm10_low  = (uint16_t)((pm10_bits >> 16U) & 0xFFFFU); /* High 16 bits (DC) */
        
        /* Sensor is READY - always reset counter (sensor is connected and responding) */
        pmsx003_zero_poll_count = 0U;
        
        /* Update last valid data */
        last_valid_pmsx003_pm1_high = pm1_high;
        last_valid_pmsx003_pm1_low = pm1_low;
        last_valid_pmsx003_pm2_5_high = pm2_5_high;
        last_valid_pmsx003_pm2_5_low = pm2_5_low;
        last_valid_pmsx003_pm10_high = pm10_high;
        last_valid_pmsx003_pm10_low = pm10_low;
        pmsx003_has_last_valid = true;
        
        /* Always send current data when sensor is READY */
        regs[MB_REG_PM1_0_HIGH] = pm1_high;
        regs[MB_REG_PM1_0_LOW]  = pm1_low;
        regs[MB_REG_PM2_5_HIGH] = pm2_5_high;
        regs[MB_REG_PM2_5_LOW]  = pm2_5_low;
        regs[MB_REG_PM10_HIGH]  = pm10_high;
        regs[MB_REG_PM10_LOW]   = pm10_low;
        
    } else {
        /* Sensor not ready (disconnected or error) */
        if (pmsx003_has_last_valid && (pmsx003_zero_poll_count < 14U)) {
            /* Send last valid data for first 14 polls after disconnect */
            regs[MB_REG_PM1_0_HIGH] = last_valid_pmsx003_pm1_high;
            regs[MB_REG_PM1_0_LOW]  = last_valid_pmsx003_pm1_low;
            regs[MB_REG_PM2_5_HIGH] = last_valid_pmsx003_pm2_5_high;
            regs[MB_REG_PM2_5_LOW]  = last_valid_pmsx003_pm2_5_low;
            regs[MB_REG_PM10_HIGH]  = last_valid_pmsx003_pm10_high;
            regs[MB_REG_PM10_LOW]   = last_valid_pmsx003_pm10_low;
            pmsx003_zero_poll_count++;
        } else {
            /* After 15 polls or no last valid, send zeros */
            regs[MB_REG_PM1_0_HIGH] = 0U;
            regs[MB_REG_PM1_0_LOW]  = 0U;
            regs[MB_REG_PM2_5_HIGH] = 0U;
            regs[MB_REG_PM2_5_LOW]  = 0U;
            regs[MB_REG_PM10_HIGH]  = 0U;
            regs[MB_REG_PM10_LOW]   = 0U;
            if (pmsx003_zero_poll_count < 15U) {
                pmsx003_zero_poll_count = 15U;
            }
        }
    }
#else
    {
        regs[MB_REG_PM1_0_HIGH] = 0U;
        regs[MB_REG_PM1_0_LOW]  = 0U;
        regs[MB_REG_PM2_5_HIGH] = 0U;
        regs[MB_REG_PM2_5_LOW]  = 0U;
        regs[MB_REG_PM10_HIGH]  = 0U;
        regs[MB_REG_PM10_LOW]   = 0U;
    }
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

#ifdef USART1_SENSOR_MODE
    if (pmsx003 != NULL) {
        regs[0x6CU] = (uint16_t)pmsx003->state;
        regs[0x6DU] = pmsx003->error_count;
    } else {
        regs[0x6CU] = 0xFFFFU;
        regs[0x6DU] = 0U;
    }
#else
    regs[0x6CU] = 0U;
    regs[0x6DU] = 0U;
#endif

    regs[MB_REG_CFG_SLAVE_ID]  = (uint16_t)g_slave_id;
    regs[MB_REG_CFG_BAUDRATE]  = (uint16_t)g_baud_code;
    regs[MB_REG_CFG_PARITY]    = (uint16_t)g_parity;
    regs[MB_REG_CFG_STOPBITS]  = (uint16_t)g_stopbits;
    
    uint32_t last_update_sec = (uint32_t)(aq->last_read_tick / 1000U);
    regs[MB_REG_CFG_LAST_UPDATE] = (uint16_t)(last_update_sec & 0xFFFFU);
    
#ifdef USART1_SENSOR_MODE
    /* PMSX003 calibration registers */
    regs[MB_REG_PMSX003_CAL_PM1_0] = g_pmsx003_cal_pm1_0;
    regs[MB_REG_PMSX003_CAL_PM2_5] = g_pmsx003_cal_pm2_5;
    regs[MB_REG_PMSX003_CAL_PM10]  = g_pmsx003_cal_pm10;
#else
    regs[MB_REG_PMSX003_CAL_PM1_0] = 1000U;
    regs[MB_REG_PMSX003_CAL_PM2_5] = 1000U;
    regs[MB_REG_PMSX003_CAL_PM10]  = 1000U;
#endif
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
    /* Exception response: NO custom header (standard Modbus only) */
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

#if defined(USART1_MODBUS_MODE) && defined(USART1_SENSOR_MODE)
    void modbus_slave_process(modbus_slave_ctx_s *ctx, const sen66_ctx_s *sensor_ctx,
                              const co_ctx_s *co_ctx, const pmsx003_ctx_s *pmsx003_ctx, uint16_t *tx_len)
#elif defined(USART1_MODBUS_MODE)
    void modbus_slave_process(modbus_slave_ctx_s *ctx, const sen66_ctx_s *sensor_ctx,
                              const co_ctx_s *co_ctx, uint16_t *tx_len)
#elif defined(USART1_SENSOR_MODE)
    void modbus_slave_process(modbus_slave_ctx_s *ctx, const sen66_ctx_s *sensor_ctx,
                              const pmsx003_ctx_s *pmsx003_ctx, uint16_t *tx_len)
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

#ifdef USART1_SENSOR_MODE
            case MB_REG_PMSX003_CAL_PM1_0:
                if ((wr_val < PMSX003_CAL_MIN) || (wr_val > PMSX003_CAL_MAX)) {
                    *tx_len = build_exception(ctx->tx_buf, fc, MB_EX_ILLEGAL_VALUE);
                    goto done;
                }
                g_pmsx003_cal_pm1_0 = wr_val;
                cfg_changed = true;
                break;

            case MB_REG_PMSX003_CAL_PM2_5:
                if ((wr_val < PMSX003_CAL_MIN) || (wr_val > PMSX003_CAL_MAX)) {
                    *tx_len = build_exception(ctx->tx_buf, fc, MB_EX_ILLEGAL_VALUE);
                    goto done;
                }
                g_pmsx003_cal_pm2_5 = wr_val;
                cfg_changed = true;
                break;

            case MB_REG_PMSX003_CAL_PM10:
                if ((wr_val < PMSX003_CAL_MIN) || (wr_val > PMSX003_CAL_MAX)) {
                    *tx_len = build_exception(ctx->tx_buf, fc, MB_EX_ILLEGAL_VALUE);
                    goto done;
                }
                g_pmsx003_cal_pm10 = wr_val;
                cfg_changed = true;
                break;
#endif

            default:
                *tx_len = build_exception(ctx->tx_buf, fc, MB_EX_ILLEGAL_ADDR);
                goto done;
        }

        if (cfg_changed) {
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

#if defined(USART1_MODBUS_MODE) && defined(USART1_SENSOR_MODE)
    build_register_map(sensor_ctx, co_ctx, pmsx003_ctx, regs);
#elif defined(USART1_MODBUS_MODE)
    build_register_map(sensor_ctx, co_ctx, regs);
#elif defined(USART1_SENSOR_MODE)
    build_register_map(sensor_ctx, pmsx003_ctx, regs);
#else
    build_register_map(sensor_ctx, regs);
#endif

    /* Custom header: 0x00 0x00 0x30 (NOT included in CRC calculation) */
    ctx->tx_buf[0U] = 0x00U;
    ctx->tx_buf[1U] = 0x00U;
    ctx->tx_buf[2U] = 0x48U;  /* 0x30 = '0' or 48 decimal */
    
    /* Build Modbus Read response */
    byte_cnt        = (uint16_t)(qty * 2U);
    ctx->tx_buf[3U] = g_slave_id;
    ctx->tx_buf[4U] = fc;
    ctx->tx_buf[5U] = (uint8_t)(byte_cnt & 0x00FFU);
    for (i = 0U; i < qty; i++) {
        uint16_t reg_val = regs[start_addr + i];
        ctx->tx_buf[6U + (i * 2U)]      = (uint8_t)((reg_val >> 8U) & 0x00FFU);
        ctx->tx_buf[6U + (i * 2U) + 1U] = (uint8_t)(reg_val & 0x00FFU);
    }
    /* CRC calculated ONLY on Modbus portion (after 0x30 header) */
    *tx_len = append_crc(&ctx->tx_buf[3U], (uint16_t)(3U + byte_cnt)) + 3U; /* +3 for header */

done:
    ctx->rx_len      = 0U;
    ctx->frame_ready = false;
}

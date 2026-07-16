/*
* @file main.c
* 
* Created on: 14 June 2026
*     Author: DST0x
*/

#include <string.h>

#include "bsp/bsp_clock.h"
#include "bsp/bsp_gpio.h"
#include "bsp/bsp_i2c.h"
#include "bsp/bsp_iwdg.h"
#include "bsp/bsp_systick.h"
#include "bsp/bsp_uart.h"

#include "middleware/modbus/modbus_slave.h"
#include "common/common_types.h"

#include "drivers/sensor_sensirion_sen66/sensirion_sen66.h"
#include "drivers/sensor_infwin_co/infwin_co_sensor.h"

#define SENSOR_POLL_MS   (1000U)
#define SENSOR_RETRY_MS  (5000U)
#define LED_BLINK_OK_MS  (500U)
#define LED_BLINK_ERR_MS (100U)
#define DEBUG_PRINT_MS   (2000U)

static sen66_ctx_s g_aqs_sensor         = {0};
static modbus_slave_ctx_s g_modbus  = {0};

#ifdef USART1_MODBUS_MODE
    static co_ctx_s g_co_sensor = {0};
#endif

#ifdef USART1_DEBUG_MODE
    static void debug_print_fixed(const char *prefix, int32_t raw, int32_t scale, uint8_t decimals, const char *suffix){
        bsp_debug_write_fixed(prefix, raw, scale, decimals, suffix);
    }

    static void debug_print_sensor(const sen66_ctx_s *s){
        const sen66_data_s *d = &s->data;
        bsp_debug_write_str("[SEN66] ");
        debug_print_fixed("PM1.0:", (int32_t)d->pm1_0, 10, 1, " | ");
        debug_print_fixed("PM2.5:", (int32_t)d->pm2_5, 10, 1, " | ");
        debug_print_fixed("PM4.0:", (int32_t)d->pm4_0, 10, 1, " | ");
        debug_print_fixed("PM10:", (int32_t)d->pm10, 10, 1, " ug/m3 | ");

        debug_print_fixed("Humidity:", (int32_t)d->rh, 10, 1, "%%RH | ");
        debug_print_fixed("Temperature:", (int32_t)d->temp, 10, 1, "C | ");

        debug_print_fixed("VOC:", (int32_t)d->voc, 10, 1, " | ");
        debug_print_fixed("NOx:", (int32_t)d->nox, 10, 1, " | ");
        debug_print_fixed("CO2:", (int32_t)d->co2_ppm, 1, 0, " ppm\r\n");
    }
#endif

static void modbus_service(void){
    uint16_t tx_len = 0U;
    while(bsp_rs485_rx_count() > 0U){
        uint8_t byte = bsp_rs485_rx_get();
        uint32_t tick = bsp_systick_get_tick();
        modbus_slave_rx_byte(&g_modbus, byte, tick);
    }

    if((g_modbus.rx_len > 0U) && bsp_systick_elapsed(g_modbus.last_rx_tick, MB_FRAME_TIMEOUT_MS)){
        #ifdef USART1_MODBUS_MODE
            modbus_slave_process(&g_modbus, &g_aqs_sensor, &g_co_sensor, &tx_len);
        #else
            modbus_slave_process(&g_modbus, &g_aqs_sensor, &tx_len);
        #endif

        if(tx_len > 0U){
            bsp_systick_delay_ms(2U);
            bsp_rs485_send(g_modbus.tx_buf, tx_len);
        }
    }
}

int main(void){
    status_e status;
    uint32_t last_poll_tick  = 0U;
    uint32_t last_led_tick   = 0U;
    uint32_t last_dbg_tick   = 0U;
    uint32_t last_retry_tick = 0U;
    bool     led_state       = false;
    bool     sensor_ok       = false;

    bsp_systick_init();
    (void)bsp_clock_init();
    bsp_gpio_init();

    #ifdef USART1_DEBUG_MODE
        bsp_gpio_set_comm_mode(COMM_MODE_TTL);
        bsp_debug_init();
        bsp_systick_delay_ms(10U);
        bsp_debug_write_str("\r\n=================================\r\n");
        bsp_debug_write_str("MERTANI BSP V1.0\r\n");
        bsp_debug_write_str("BE PART OF NATURE PROTECT THE FUTURE\r\n");
        bsp_debug_write_str("\r\n=================================\r\n");
    #endif

    #ifdef USART1_MODBUS_MODE
        bsp_gpio_set_comm_mode(COMM_MODE_RS485);
    #endif

    bsp_i2c_init();
    bsp_rs485_init();
    bsp_iwdg_init();
    modbus_slave_init(&g_modbus);

    {
        uint8_t attempt;
        status = STATUS_ERR_GENERIC;
        for (attempt = 0U; attempt < 3U; attempt++)
        {
            bsp_iwdg_refresh();
            status = sensirion_sen66_init(&g_aqs_sensor);
            if (status == STATUS_OK){
                break;
            }
        #ifdef USART1_DEBUG_MODE
            bsp_debug_write_str("SEN66 init failed, retrying...\r\n");
        #endif

            bsp_systick_delay_ms(500U);
        }
        bsp_iwdg_refresh();

        if (status == STATUS_OK){
            status = sensirion_sen66_start_measurement(&g_aqs_sensor);
            if (status == STATUS_OK){
                sensor_ok = true;
                #ifdef USART1_DEBUG_MODE
                    bsp_debug_write_str("SEN66 OK. Warming up 60s...\r\n");
                #endif
            }
            #ifdef USART1_DEBUG_MODE
                else { bsp_debug_write_str("SEN66 start failed.\r\n"); }
            #endif
        }
        #ifdef USART1_DEBUG_MODE
            else { bsp_debug_write_str("SEN66 not detected.\r\n"); }
        #endif
    }

 
    #ifdef USART1_SENSOR_MODE
        {
            status = sensor_co_init(&g_co_sensor);
            (void)status;
        }
    #endif


    last_poll_tick  = bsp_systick_get_tick();
    last_led_tick   = bsp_systick_get_tick();
    last_dbg_tick   = bsp_systick_get_tick();
    last_retry_tick = bsp_systick_get_tick();

    for (;;){
        uint32_t now = bsp_systick_get_tick();
        if (sensor_ok && bsp_systick_elapsed(last_poll_tick, SENSOR_POLL_MS)){
            last_poll_tick = now;
            if (sensirion_sen66_is_data_ready(&g_aqs_sensor)){
                status = sensirion_sen66_poll(&g_aqs_sensor);
                if ((status != STATUS_OK) && (status != STATUS_NOT_READY) && (status != STATUS_ERR_STATE)){
                #ifdef USART1_DEBUG_MODE
                    bsp_debug_write_str("SEN66 offline.\r\n");
                #endif
                    sensor_ok = false;
                    last_retry_tick = now;
                    (void)memset(&g_aqs_sensor.data, 0, sizeof(g_aqs_sensor.data));
                    g_aqs_sensor.data_fresh = false;
                }
            }
        }

        if (!sensor_ok && bsp_systick_elapsed(last_retry_tick, SENSOR_RETRY_MS)){
            last_retry_tick = now;
            bsp_iwdg_refresh();
            status = sensirion_sen66_init(&g_aqs_sensor);
            if (status == STATUS_OK){
                status = sensirion_sen66_start_measurement(&g_aqs_sensor);
                if (status == STATUS_OK){
                    sensor_ok = true;
                    last_poll_tick = now;
                #ifdef USART1_DEBUG_MODE
                    bsp_debug_write_str("SEN66 reconnected.\r\n");
                #endif
                }
            }
            bsp_iwdg_refresh();
        }

        #ifdef USART1_MODBUS_MODE
                (void)sensor_co_poll(&g_co_sensor);
        #endif
        modbus_service();
        {
            uint32_t blink = (g_aqs_sensor.state == SEN66_STATE_ERROR) ? LED_BLINK_ERR_MS : LED_BLINK_OK_MS;
            if (bsp_systick_elapsed(last_led_tick, blink)){
                last_led_tick = now;
                led_state = !led_state;
                if (led_state) { bsp_gpio_led_on();  }
                else           { bsp_gpio_led_off(); }
            }
        }

        #ifdef USART1_DEBUG_MODE
            if (bsp_systick_elapsed(last_dbg_tick, DEBUG_PRINT_MS)){
            last_dbg_tick = now;
            if (g_aqs_sensor.data_fresh) { debug_print_sensor(&g_aqs_sensor); }
                }
        #else
            (void)last_dbg_tick;
        #endif
        bsp_iwdg_refresh();
    }
    return 0;
}
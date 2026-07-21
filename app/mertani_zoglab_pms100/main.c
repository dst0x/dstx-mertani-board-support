/*
 * @file main.c
 * @brief App: SEN66 (I2C) + PMSX003 particulate sensor (USART1)
 *
 * Build/flash with: make pmsx003_only
 */

#include <string.h>

#include "bsp/bsp_clock.h"
#include "bsp/bsp_gpio.h"
#include "bsp/bsp_i2c.h"
#include "bsp/bsp_iwdg.h"
#include "bsp/bsp_systick.h"
#include "bsp/bsp_uart.h"

#include "middleware/modbus/modbus_slave.h"
#include "middleware/sensor_manager.h"
#include "middleware/uart_manager.h"
#include "common/common_types.h"

#include "drivers/sensor_sensirion_sen66/sensirion_sen66.h"
#include "drivers/sensor_pmsx003/pmsx003_sensor.h"

#define LED_BLINK_OK_MS  (500U)
#define LED_BLINK_ERR_MS (100U)
#define DEBUG_PRINT_MS   (2000U)

static sen66_ctx_s g_aqs_sensor = {0};
static pmsx003_ctx_s g_pmsx003_sensor = {0};
static modbus_slave_ctx_s g_modbus = {0};
static sensor_manager_ctx_s g_sensor_manager = {0};
static uart_manager_ctx_s g_uart_manager = {0};

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

        debug_print_fixed("Humidity:", (int32_t)d->rh, 10, 1, "RH | ");
        debug_print_fixed("Temperature:", (int32_t)d->temp, 10, 1, "C | ");

        debug_print_fixed("VOC:", (int32_t)d->voc, 10, 1, " | ");
        debug_print_fixed("NOx:", (int32_t)d->nox, 10, 1, " | ");
        debug_print_fixed("CO2:", (int32_t)d->co2_ppm, 1, 0, " ppm");

        if (!d->co2_valid) {
            bsp_debug_write_str(" [UNAVAIL]");
        }
        bsp_debug_write_str(" |\r\n");
    }

    static void debug_print_status(void) {
        sensor_status_e sen66_status = sensor_manager_get_status(&g_sensor_manager, 0);
        sensor_status_e pmsx003_status = sensor_manager_get_status(&g_sensor_manager, 2);
        bsp_debug_write_str("[STATUS] SEN66: ");

        switch (sen66_status) {
            case SENSOR_STATUS_RUNNING:     bsp_debug_write_str("RUNNING"); break;
            case SENSOR_STATUS_WARMING_UP:  bsp_debug_write_str("WARMING_UP"); break;
            case SENSOR_STATUS_ERROR:       bsp_debug_write_str("ERROR"); break;
            case SENSOR_STATUS_INITIALIZING:bsp_debug_write_str("INITIALIZING"); break;
            default:                        bsp_debug_write_str("UNKNOWN"); break;
        }

        uint32_t errors, zeros;
        sensor_manager_get_stats(&g_sensor_manager, 0, &errors, &zeros);
        bsp_debug_write_str(" | Errors: ");
        bsp_debug_write_int("", (int32_t)errors, "");
        bsp_debug_write_str(" | Zeros: ");
        bsp_debug_write_int("", (int32_t)zeros, "|\r\n");

        bsp_debug_write_str("[STATUS] PMSX003: ");
        switch (pmsx003_status) {
            case SENSOR_STATUS_RUNNING:     bsp_debug_write_str("RUNNING"); break;
            case SENSOR_STATUS_WARMING_UP:  bsp_debug_write_str("WARMING_UP"); break;
            case SENSOR_STATUS_ERROR:       bsp_debug_write_str("ERROR"); break;
            case SENSOR_STATUS_INITIALIZING:bsp_debug_write_str("INITIALIZING"); break;
            default:                        bsp_debug_write_str("UNKNOWN"); break;
        }
        bsp_debug_write_str(" |\r\n");
    }
#endif

static void modbus_service(void) {
    uint16_t tx_len = 0U;
    while(bsp_rs485_rx_count() > 0U) {
        uint8_t byte = bsp_rs485_rx_get();
        uint32_t tick = bsp_systick_get_tick();
        modbus_slave_rx_byte(&g_modbus, byte, tick);
    }

    if((g_modbus.rx_len > 0U) && bsp_systick_elapsed(g_modbus.last_rx_tick, MB_FRAME_TIMEOUT_MS)) {
        modbus_slave_process(&g_modbus, &g_aqs_sensor, &g_pmsx003_sensor, &tx_len);

        if(tx_len > 0U) {
            bsp_systick_delay_ms(2U);
            bsp_rs485_send(g_modbus.tx_buf, tx_len);
        }
    }
}

int main(void) {
    uint32_t last_led_tick   = 0U;
    uint32_t last_dbg_tick   = 0U;
    bool     led_state       = false;

    bsp_systick_init();
    (void)bsp_clock_init();
    bsp_gpio_init();
    bsp_i2c_init();
    bsp_iwdg_init();

    (void)uart_manager_init(&g_uart_manager);

    modbus_slave_init(&g_modbus);

    g_sensor_manager.sen66_driver = &g_aqs_sensor;
    g_sensor_manager.pmsx003_driver = &g_pmsx003_sensor;
    bsp_sensor_set_pmsx003_ptr(&g_pmsx003_sensor);  /* Register PMSX003 sensor for UART RX interrupt */

    (void)sensor_manager_init(&g_sensor_manager);

    #ifdef USART1_DEBUG_MODE
        bsp_debug_write_str("\r\n[MAIN] pmsx003_only app initialized.\r\n");
        bsp_debug_write_str("[MAIN] Sensors: SEN66 + PMSX003\r\n");
    #endif

    last_led_tick = bsp_systick_get_tick();
    last_dbg_tick = bsp_systick_get_tick();

    for (;;) {
        uint32_t now = bsp_systick_get_tick();

        (void)sensor_manager_poll(&g_sensor_manager);

        modbus_service();

        {
            sensor_status_e sen66_status = sensor_manager_get_status(&g_sensor_manager, 0);
            sensor_status_e pmsx003_status = sensor_manager_get_status(&g_sensor_manager, 2);

            bool any_error = (sen66_status == SENSOR_STATUS_ERROR) || (pmsx003_status == SENSOR_STATUS_ERROR);
            uint32_t blink = any_error ? LED_BLINK_ERR_MS : LED_BLINK_OK_MS;

            if (bsp_systick_elapsed(last_led_tick, blink)) {
                last_led_tick = now;
                led_state = !led_state;
                if (led_state) {
                    bsp_gpio_led_on();
                } else {
                    bsp_gpio_led_off();
                }
            }
        }

        #ifdef USART1_DEBUG_MODE
            if (bsp_systick_elapsed(last_dbg_tick, DEBUG_PRINT_MS)) {
                last_dbg_tick = now;

                if (g_aqs_sensor.data_fresh) {
                    debug_print_sensor(&g_aqs_sensor);
                }

                debug_print_status();
            }
        #else
            (void)last_dbg_tick;
        #endif

        bsp_iwdg_refresh();
    }

    return 0;
}

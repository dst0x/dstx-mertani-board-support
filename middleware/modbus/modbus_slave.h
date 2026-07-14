/*
* @file modbus_slave.h
* 
* Created on: 14 June 2026
*     Author: DST0x
* FC03/FC04 Read Holding/Input Registers (read-only):
*   Addr  Description          Scale    Source
*   0x00  PM1.0  µg/m³         ÷10      SEN66
*   0x01  PM2.5  µg/m³         ÷10      SEN66
*   0x02  PM4.0  µg/m³         ÷10      SEN66
*   0x03  PM10   µg/m³         ÷10      SEN66
*   0x04  RH     %RH           ÷100     SEN66
*   0x05  Temp   °C            ÷10      SEN66
*   0x06  VOC index            ÷10      SEN66
*   0x07  NOx index            ÷10      SEN66
*   0x08  CO2    ppm           ×1       SEN66
*   0x09  NC0.5 #/cm³         ÷10      SEN66
*   0x0A  NC1.0 #/cm³         ÷10      SEN66
*   0x0B  NC2.5 #/cm³         ÷10      SEN66
*   0x0C  NC4.0 #/cm³         ÷10      SEN66
*   0x0D  NC10  #/cm³         ÷10      SEN66
*   0x0E  Status flags         bitmask  SEN66
*   0x0F  Sensor state         0-4      SEN66
*   0x10  Error count          —        SEN66
*   0x11  CO ppm               ×1       Infwin
*   0x12  CO request count     —        Debug
*   0x13  CO response count    —        Debug
*   0x14  CO timeout count     —        Debug
*   0x15  CO CRC error count   —        Debug
*/

#ifndef MODBUS_SLAVE_H
#define MODBUS_SLAVE_H

#include <stddef.h>
#include <string.h>

#include "bsp/bsp_systick.h"
#include "bsp/bsp_uart.h"
#include "common/common_types.h"
#include "drivers/sensor_sensirion_sen66/sensirion_sen66.h"
#include "drivers/sensor_infwin_co/infwin_co_sensor.h"
#include "middleware/modbus/modbus_crc.h"

#ifndef MB_SLAVE_ID
#define MB_SLAVE_ID         (0x01U)
#endif

#define MB_FRAME_TIMEOUT_MS (5U)
#define MB_RX_BUF_SIZE      (32U)
#define MB_TX_BUF_SIZE      (64U)
#define MB_REG_FIRST        (0x0000U)
#define MB_REG_LAST         (0x0015U)
#define MB_REG_COUNT        (MB_REG_LAST - MB_REG_FIRST + 1U)
#define MB_FC_READ_HOLDING  (0x03U)
#define MB_FC_READ_INPUT    (0x04U)
#define MB_EX_ILLEGAL_FUNC  (0x01U)
#define MB_EX_ILLEGAL_ADDR  (0x02U)
#define MB_EX_ILLEGAL_VALUE (0x03U)

typedef struct modbus_slave_ctx_tag{
    uint32_t last_rx_tick;
    uint16_t rx_len;
    bool     frame_ready;
    uint8_t  rx_buf[MB_RX_BUF_SIZE];
    uint8_t  tx_buf[MB_TX_BUF_SIZE];
}modbus_slave_ctx_s;

void modbus_slave_init(modbus_slave_ctx_s *ctx);
void modbus_slave_rx_byte(modbus_slave_ctx_s *ctx, uint8_t byte, uint32_t tick);

#ifdef USART1_MODBUS_MODE
    void modbus_slave_process(modbus_slave_ctx_s *ctx, const sen66_ctx_s *sensor_ctx, const co_ctx_s *co_ctx, uint16_t *tx_len);
#else
    void modbus_slave_process(modbus_slave_ctx_s *ctx, const sen66_ctx_s *sensor_ctx, uint16_t *tx_len);
#endif /* USART1_MODBUS_MODE */
#endif /* MODBUS_SLAVE_H*/
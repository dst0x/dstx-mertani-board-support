/*
* @file modbus_slave.h
* 
* Created on: 14 June 2026
*     Author: DST0x
*
* Modbus Register Map (FC03 / FC04 Read Holding/Input Registers):
*
* ---- SENSOR DATA (0x00 - 0x0F) ----------------------------------------
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
*   0x09  CO     ppm           ×1       Infwin
*   0x0A  (reserved)
*   0x0B  (reserved)
*   0x0C  (reserved)
*   0x0D  (reserved)
*   0x0E  (reserved)
*   0x0F  (reserved)
*
* ---- DEBUG / STATUS (0x64 = 100 onwards) --------------------------------
*   0x64  SEN66 state          0-4      SEN66
*   0x65  SEN66 error count    —        SEN66
*   0x66  SEN66 status flags   bitmask  SEN66 (pm|env|gas|co2 valid)
*   0x67  CO sensor state      0-4      Infwin
*   0x68  CO request count     —        Infwin
*   0x69  CO response count    —        Infwin
*   0x6A  CO timeout count     —        Infwin
*   0x6B  CO CRC error count   —        Infwin
*
* ---- CONFIG / WRITABLE (0xF0 = 240 onwards) -----------------------------
*   Addr  Description          Range         Notes
*   0xF0  Slave ID             1-247         R/W via FC06; takes effect immediately
*   0xF1  Baudrate             see below     R/W via FC06; takes effect after write
*          Valid values: 1=1200, 2=2400, 3=4800, 4=9600, 5=19200,
*                        6=38400, 7=57600, 8=115200
*   0xF2  Parity               0=None        R/W via FC06; takes effect after write
*                              1=Even
*                              2=Odd
*   0xF3  Stop bits            1=1 stop bit  R/W via FC06; takes effect after write
*                              2=2 stop bits
*   0xF4  Last update          seconds       Read-only; seconds since boot of last sensor data update
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
/* TX buffer: worst case = 3 (header) + MB_REG_COUNT*2 (data) + 2 (CRC) = 487 bytes.
 * Set to 512 to cover full register range read in a single FC03 request. */
#define MB_TX_BUF_SIZE      (512U)

/* Register map boundaries */
#define MB_REG_DATA_FIRST   (0x0000U)   /* First sensor data register */
#define MB_REG_DATA_LAST    (0x0009U)   /* Last sensor data register */

#define MB_REG_DEBUG_FIRST  (0x0064U)   /* First debug register (100) */
#define MB_REG_DEBUG_LAST   (0x006BU)   /* Last debug register (107) */

/* Config registers (writable via FC06) */
#define MB_REG_CFG_FIRST        (0x00F0U)   /* First config register (240) */
#define MB_REG_CFG_SLAVE_ID     (0x00F0U)   /* Slave ID       — write 1-247       */
#define MB_REG_CFG_BAUDRATE     (0x00F1U)   /* Baudrate code  — write 1-8 (see map)*/
#define MB_REG_CFG_PARITY       (0x00F2U)   /* Parity         — 0=None,1=Even,2=Odd*/
#define MB_REG_CFG_STOPBITS     (0x00F3U)   /* Stop bits      — 1 or 2            */
#define MB_REG_CFG_LAST_UPDATE  (0x00F4U)   /* Last update timestamp (seconds since boot) — Read-only */
#define MB_REG_CFG_LAST         (0x00F4U)   /* Last config register (244) */

/* Baudrate code → actual bps mapping (stored in register 0xF1) */
#define MB_BAUD_CODE_1200       (1U)
#define MB_BAUD_CODE_2400       (2U)
#define MB_BAUD_CODE_4800       (3U)
#define MB_BAUD_CODE_9600       (4U)
#define MB_BAUD_CODE_19200      (5U)
#define MB_BAUD_CODE_38400      (6U)
#define MB_BAUD_CODE_57600      (7U)
#define MB_BAUD_CODE_115200     (8U)
#define MB_BAUD_CODE_MIN        (MB_BAUD_CODE_1200)
#define MB_BAUD_CODE_MAX        (MB_BAUD_CODE_115200)

/* Parity values */
#define MB_PARITY_NONE          (0U)
#define MB_PARITY_EVEN          (1U)
#define MB_PARITY_ODD           (2U)

/* Stop bit values */
#define MB_STOPBITS_1           (1U)
#define MB_STOPBITS_2           (2U)

/* Total register space: 0x00 to 0xF4 = 245 registers */
#define MB_REG_FIRST            (0x0000U)
#define MB_REG_LAST             (0x00F4U)
#define MB_REG_COUNT            (MB_REG_LAST - MB_REG_FIRST + 1U)

#define MB_SLAVE_ID_MIN     (0x01U)     /* Minimum valid slave ID */
#define MB_SLAVE_ID_MAX     (0xF7U)     /* Maximum valid slave ID (247) */

#define MB_FC_READ_HOLDING  (0x03U)
#define MB_FC_READ_INPUT    (0x04U)
#define MB_FC_WRITE_SINGLE  (0x06U)     /* Write Single Register */
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
#endif

#endif /* MODBUS_SLAVE_H */

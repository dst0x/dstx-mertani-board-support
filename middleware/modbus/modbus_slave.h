/*
* @file modbus_slave.h
* 
* Created on: 14 June 2026
*     Author: DST0x
*
* Modbus Register Map (FC03 / FC04 Read Holding/Input Registers):
*
* ---- SENSOR DATA (0x00 - 0x1F) ----------------------------------------
*   Addr  Description          Scale    Source    Notes
*   0x00  PM1.0  µg/m³         ÷10      SEN66     
*   0x01  PM2.5  µg/m³         ÷10      SEN66     
*   0x02  PM4.0  µg/m³         ÷10      SEN66     
*   0x03  PM10   µg/m³         ÷10      SEN66     
*   0x04  RH     %RH           ÷10      SEN66     
*   0x05  Temp   °C            ÷10      SEN66     (sensor ×200 → driver ÷20 → reg)
*   0x06  VOC index            ×1       SEN66     (direct value, no scaling)
*   0x07  NOx index            ×1       SEN66     (direct value, no scaling)
*   0x08  CO2    ppm           ×1       SEN66     
*   0x09  CO     ppm           ×1       Infwin    
*   0x0A  (reserved)
*   0x0B  (reserved)
*   0x0C  (reserved)
*   0x0D  (reserved)
*   0x0E  (reserved)
*   0x0F  (reserved)
*
* ---- PMSX003 DATA (0x0410 - 0x0415) IEEE 754 Float CDAB Byte Order ----
*   Addr  Description          Format            Source
*   0x0410  PM1.0 MSW          IEEE 754 (CDAB)   PMSX003
*   0x0411  PM1.0 LSW          IEEE 754 (CDAB)   PMSX003
*   0x0412  PM2.5 MSW          IEEE 754 (CDAB)   PMSX003
*   0x0413  PM2.5 LSW          IEEE 754 (CDAB)   PMSX003
*   0x0414  PM10  MSW          IEEE 754 (CDAB)   PMSX003
*   0x0415  PM10  LSW          IEEE 754 (CDAB)   PMSX003
*
* Note: CDAB byte order (middle-endian) means:
*   Float bytes [A B C D] stored as register pairs [C D] [A B]
*   MSW = Most Significant Word (bytes C D)
*   LSW = Least Significant Word (bytes A B)
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
*   0x6C  PMSX003 state        0-4      PMSX003
*   0x6D  PMSX003 error count  —        PMSX003
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
*
* ---- PMSX003 CALIBRATION (0xF5-0xF7 = 245-247) --------------------------
*   Addr  Description          Range         Notes
*   0xF5  PM1.0 correction     100-10000     R/W via FC06; fixed-point ×1000 (e.g., 1000 = 1.0x, 1500 = 1.5x)
*   0xF6  PM2.5 correction     100-10000     R/W via FC06; fixed-point ×1000
*   0xF7  PM10 correction      100-10000     R/W via FC06; fixed-point ×1000
*   Default: 1000 (1.0x, no correction). Values persist to Flash.
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
#include "drivers/sensor_pmsx003/pmsx003_sensor.h"
#include "middleware/modbus/modbus_crc.h"

#ifndef MB_SLAVE_ID
#define MB_SLAVE_ID         (0x01U)
#endif

#define MB_FRAME_TIMEOUT_MS (5U)
#define MB_RX_BUF_SIZE      (32U)
#define MB_TX_BUF_SIZE      (512U)

#define MB_REG_DATA_FIRST   (0x0000U)   
#define MB_REG_DATA_LAST    (0x0009U)   

#define MB_REG_PMSX003_BASE     (0x0410U)  
#define MB_REG_PM1_0_HIGH       (0x0410U)   
#define MB_REG_PM1_0_LOW        (0x0411U)  
#define MB_REG_PM2_5_HIGH       (0x0412U)  
#define MB_REG_PM2_5_LOW        (0x0413U)   
#define MB_REG_PM10_HIGH        (0x0414U)  
#define MB_REG_PM10_LOW         (0x0415U)   
#define MB_REG_PMSX003_COUNT    (6U)        

#define MB_REG_DEBUG_FIRST  (0x0064U)   
#define MB_REG_DEBUG_LAST   (0x006DU)   

#define MB_REG_CFG_FIRST        (0x00F0U)  
#define MB_REG_CFG_SLAVE_ID     (0x00F0U)   
#define MB_REG_CFG_BAUDRATE     (0x00F1U)  
#define MB_REG_CFG_PARITY       (0x00F2U)  
#define MB_REG_CFG_STOPBITS     (0x00F3U)   
#define MB_REG_CFG_LAST_UPDATE  (0x00F4U)  

#define MB_REG_PMSX003_CAL_PM1_0    (0x00F5U)  /* PM1.0 correction factor */
#define MB_REG_PMSX003_CAL_PM2_5    (0x00F6U)  /* PM2.5 correction factor */
#define MB_REG_PMSX003_CAL_PM10     (0x00F7U)  /* PM10 correction factor */

#define MB_REG_CFG_LAST         (0x00F7U)

/* Baudrate */
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

/* Parity */
#define MB_PARITY_NONE          (0U)
#define MB_PARITY_EVEN          (1U)
#define MB_PARITY_ODD           (2U)

/* Stop Bit*/
#define MB_STOPBITS_1           (1U)
#define MB_STOPBITS_2           (2U)

#define MB_REG_FIRST            (0x0000U)
#define MB_REG_LAST             (0x0415U)   
#define MB_REG_COUNT            (0x0416U)   

#define MB_SLAVE_ID_MIN     (0x01U)     
#define MB_SLAVE_ID_MAX     (0xF7U)     

#define MB_FC_READ_HOLDING  (0x03U)
#define MB_FC_READ_INPUT    (0x04U)
#define MB_FC_WRITE_SINGLE  (0x06U)     
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

#if defined(USART1_MODBUS_MODE) && defined(USART1_SENSOR_MODE)
    void modbus_slave_process(modbus_slave_ctx_s *ctx, const sen66_ctx_s *sensor_ctx, const co_ctx_s *co_ctx, const pmsx003_ctx_s *pmsx003_ctx, uint16_t *tx_len);
#elif defined(USART1_MODBUS_MODE)
    void modbus_slave_process(modbus_slave_ctx_s *ctx, const sen66_ctx_s *sensor_ctx, const co_ctx_s *co_ctx, uint16_t *tx_len);
#elif defined(USART1_SENSOR_MODE)
    void modbus_slave_process(modbus_slave_ctx_s *ctx, const sen66_ctx_s *sensor_ctx, const pmsx003_ctx_s *pmsx003_ctx, uint16_t *tx_len);
#else
    void modbus_slave_process(modbus_slave_ctx_s *ctx, const sen66_ctx_s *sensor_ctx, uint16_t *tx_len);
#endif

#endif /* MODBUS_SLAVE_H */

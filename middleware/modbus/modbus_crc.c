#include "middleware/modbus/modbus_crc.h"

uint16_t modbus_crc16(const uint8_t *buf, uint16_t len){
    uint16_t crc = 0xFFFFU;
    uint16_t i;
    uint8_t  bit;

    if (buf == NULL) { return 0U; }

    for (i = 0U; i < len; i++)
    {
        crc ^= (uint16_t)buf[i];
        for (bit = 8U; bit > 0U; bit--)
        {
            if ((crc & 0x0001U) != 0U)
            {
                crc = (uint16_t)((crc >> 1U) ^ 0xA001U);
            }
            else
            {
                crc >>= 1U;
            }
        }
    }
    return crc;
}
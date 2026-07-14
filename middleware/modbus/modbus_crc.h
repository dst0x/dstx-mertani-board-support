#ifndef MODBUS_CRC_H
#define MODBUS_CRC_H

#include "common/common_types.h"

uint16_t modbus_crc16(const uint8_t *buf, uint16_t len);

#endif /* MODBUS_CRC_H */
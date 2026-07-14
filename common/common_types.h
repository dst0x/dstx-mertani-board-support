#ifndef COMMON_TYPES_H
#define COMMON_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef enum status_tag {
    STATUS_OK           =  0,
    STATUS_ERR_TIMEOUT  = -1,
    STATUS_ERR_CRC      = -2,
    STATUS_ERR_I2C      = -3,
    STATUS_ERR_PARAM    = -4,
    STATUS_ERR_STATE    = -5,
    STATUS_ERR_SENSOR   = -6,
    STATUS_NOT_READY    = -7,
    STATUS_ERR_GENERIC  = -8
} status_e;

#define ARRAY_SIZE(arr)         ((uint32_t)(sizeof(arr) / sizeof((arr)[0U])))
#define UNUSED(x)               ((void)(x))

/* Bit manipulation macros - defined in stm32g0xx.h (CMSIS).
 * If CMSIS headers are not included, define them here as fallback. */
#ifndef SET_BIT
#define SET_BIT(reg, bit)       ((reg) |=  (uint32_t)(bit))
#endif

#ifndef CLR_BIT
#define CLR_BIT(reg, bit)       ((reg) &= ~(uint32_t)(bit))
#endif

#ifndef TST_BIT
#define TST_BIT(reg, bit)       (((reg) & (uint32_t)(bit)) != 0UL)
#endif

#ifndef MODIFY_REG
#define MODIFY_REG(reg, msk, val) ((reg) = (((reg) & ~(uint32_t)(msk)) | (uint32_t)(val)))
#endif

/* Data conversion macros */
#define BYTES_TO_U16(msb, lsb)  (((uint16_t)(msb) << 8U) | (uint16_t)(lsb))
#define BYTES_TO_I16(msb, lsb)  ((int16_t)(BYTES_TO_U16((msb),(lsb))))

/* Clock configuration for STM32G031F8 */
#define HSI_VALUE_HZ            (16000000UL)  /* Internal oscillator: 16 MHz */
#define SYSCLK_HZ               (64000000UL)  /* System clock (PLL): 64 MHz */
#define PCLK_HZ                 (64000000UL)  /* Peripheral clock: 64 MHz */

#endif /* COMMON_TYPES_H */
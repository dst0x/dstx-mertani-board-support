#include "bsp/bsp_gpio.h"
#include "stm32g0xx.h"

#define PIN_MODE_AF(p)     (2UL << ((p) * 2U))
#define PIN_MODE_OUT(p)    (1UL << ((p) * 2U))
#define PIN_MODE_MASK(p)   (3UL << ((p) * 2U))
#define PIN_OT_OD(p)       (1UL << (p))
#define PIN_OSPEED_HIGH(p) (2UL << ((p) * 2U))
#define PIN_OSPEED_MASK(p) (3UL << ((p) * 2U))
#define PIN_PUPD_PU(p)     (1UL << ((p) * 2U))
#define PIN_PUPD_MASK(p)   (3UL << ((p) * 2U))
#define AFR_SET(p, af)     ((uint32_t)(af) << (((p) % 8U) * 4U))
#define AFR_MASK(p)        (0xFUL << (((p) % 8U) * 4U))

#define PIN_DEMUX_A     (0U)
#define PIN_DEMUX_B     (1U)
#define PIN_USART2_TX   (2U)
#define PIN_USART2_RX   (3U)
#define PIN_LED         (4U)
#define PIN_I2C_SCL     (11U)
#define PIN_I2C_SDA     (12U)
#define PIN_RS485_DE    (1U)
#define PIN_USART1_TX   (6U)
#define PIN_USART1_RX   (7U)

#define AF_I2C2      (6U)
#define AF_USART1    (0U)
#define AF_USART2    (1U)

void bsp_gpio_init(void)
{
    SET_BIT(RCC->IOPENR, RCC_IOPENR_GPIOAEN | RCC_IOPENR_GPIOBEN);
    (void)RCC->IOPENR;

    /* SP4T DeMux Control — PA0/PA1, push-pull output, start 11 (TTL mode for debug) */
    GPIOA->MODER  &= ~(PIN_MODE_MASK(PIN_DEMUX_A) | PIN_MODE_MASK(PIN_DEMUX_B));
    GPIOA->MODER  |=  (PIN_MODE_OUT(PIN_DEMUX_A)  | PIN_MODE_OUT(PIN_DEMUX_B));
    GPIOA->OTYPER &= ~(PIN_OT_OD(PIN_DEMUX_A)     | PIN_OT_OD(PIN_DEMUX_B));
    GPIOA->ODR    |=  ((1UL << PIN_DEMUX_A) | (1UL << PIN_DEMUX_B));  /* 11 = TTL (debug mode) */

    /* USART2 TX/RX — PA2/PA3, AF1, push-pull (RS485) */
    GPIOA->MODER   &= ~(PIN_MODE_MASK(PIN_USART2_TX) | PIN_MODE_MASK(PIN_USART2_RX));
    GPIOA->MODER   |=  (PIN_MODE_AF(PIN_USART2_TX)   | PIN_MODE_AF(PIN_USART2_RX));
    GPIOA->OTYPER  &= ~(PIN_OT_OD(PIN_USART2_TX)     | PIN_OT_OD(PIN_USART2_RX));
    GPIOA->OSPEEDR &= ~(PIN_OSPEED_MASK(PIN_USART2_TX) | PIN_OSPEED_MASK(PIN_USART2_RX));
    GPIOA->OSPEEDR |=  (PIN_OSPEED_HIGH(PIN_USART2_TX) | PIN_OSPEED_HIGH(PIN_USART2_RX));
    GPIOA->PUPDR   &= ~(PIN_PUPD_MASK(PIN_USART2_TX) | PIN_PUPD_MASK(PIN_USART2_RX));
    GPIOA->PUPDR   |=   PIN_PUPD_PU(PIN_USART2_RX);
    GPIOA->AFR[0]  &= ~(AFR_MASK(PIN_USART2_TX) | AFR_MASK(PIN_USART2_RX));
    GPIOA->AFR[0]  |=  (AFR_SET(PIN_USART2_TX, AF_USART2) | AFR_SET(PIN_USART2_RX, AF_USART2));

    /* LED — PA4, push-pull output, start OFF */
    GPIOA->MODER  &= ~PIN_MODE_MASK(PIN_LED);
    GPIOA->MODER  |=  PIN_MODE_OUT(PIN_LED);
    GPIOA->OTYPER &= ~PIN_OT_OD(PIN_LED);
    GPIOA->ODR    &= ~(1UL << PIN_LED);

    /* I2C2 SCL/SDA — PA11/PA12, AF6, open-drain, pull-up */
    GPIOA->MODER   &= ~(PIN_MODE_MASK(PIN_I2C_SCL) | PIN_MODE_MASK(PIN_I2C_SDA));
    GPIOA->MODER   |=  (PIN_MODE_AF(PIN_I2C_SCL)   | PIN_MODE_AF(PIN_I2C_SDA));
    GPIOA->OTYPER  |=  (PIN_OT_OD(PIN_I2C_SCL)     | PIN_OT_OD(PIN_I2C_SDA));
    GPIOA->OSPEEDR &= ~(PIN_OSPEED_MASK(PIN_I2C_SCL) | PIN_OSPEED_MASK(PIN_I2C_SDA));
    GPIOA->OSPEEDR |=  (PIN_OSPEED_HIGH(PIN_I2C_SCL) | PIN_OSPEED_HIGH(PIN_I2C_SDA));
    GPIOA->PUPDR   &= ~(PIN_PUPD_MASK(PIN_I2C_SCL) | PIN_PUPD_MASK(PIN_I2C_SDA));
    GPIOA->PUPDR   |=  (PIN_PUPD_PU(PIN_I2C_SCL)   | PIN_PUPD_PU(PIN_I2C_SDA));
    GPIOA->AFR[1]  &= ~(AFR_MASK(PIN_I2C_SCL) | AFR_MASK(PIN_I2C_SDA));
    GPIOA->AFR[1]  |=  (AFR_SET(PIN_I2C_SCL, AF_I2C2) | AFR_SET(PIN_I2C_SDA, AF_I2C2));

    /* RS485 Mode Control — PB1, push-pull output, start LOW (Logging mode) */
    GPIOB->MODER  &= ~PIN_MODE_MASK(PIN_RS485_DE);
    GPIOB->MODER  |=  PIN_MODE_OUT(PIN_RS485_DE);
    GPIOB->OTYPER &= ~PIN_OT_OD(PIN_RS485_DE);
    GPIOB->ODR    &= ~(1UL << PIN_RS485_DE);  /* Default: Logging Mode (LOW) */

    /* USART1 TX/RX — PB6/PB7, AF0, push-pull (Debug) */
    GPIOB->MODER   &= ~(PIN_MODE_MASK(PIN_USART1_TX) | PIN_MODE_MASK(PIN_USART1_RX));
    GPIOB->MODER   |=  (PIN_MODE_AF(PIN_USART1_TX)   | PIN_MODE_AF(PIN_USART1_RX));
    GPIOB->OTYPER  &= ~(PIN_OT_OD(PIN_USART1_TX)     | PIN_OT_OD(PIN_USART1_RX));
    GPIOB->OSPEEDR &= ~(PIN_OSPEED_MASK(PIN_USART1_TX) | PIN_OSPEED_MASK(PIN_USART1_RX));
    GPIOB->OSPEEDR |=  (PIN_OSPEED_HIGH(PIN_USART1_TX) | PIN_OSPEED_HIGH(PIN_USART1_RX));
    GPIOB->PUPDR   &= ~(PIN_PUPD_MASK(PIN_USART1_TX) | PIN_PUPD_MASK(PIN_USART1_RX));
    GPIOB->PUPDR   |=   PIN_PUPD_PU(PIN_USART1_RX);
    GPIOB->AFR[0]  &= ~(AFR_MASK(PIN_USART1_TX) | AFR_MASK(PIN_USART1_RX));
    GPIOB->AFR[0]  |=  (AFR_SET(PIN_USART1_TX, AF_USART1) | AFR_SET(PIN_USART1_RX, AF_USART1));
}

void bsp_gpio_led_on(void)     { GPIOA->BSRR = (1UL << PIN_LED); }
void bsp_gpio_led_off(void)    { GPIOA->BSRR = (1UL << (PIN_LED + 16U)); }
void bsp_gpio_led_toggle(void) { GPIOA->ODR  ^= (1UL << PIN_LED); }
void bsp_gpio_rs485_drive_mode(void)   { GPIOB->BSRR = (1UL << PIN_RS485_DE); }  
void bsp_gpio_rs485_logging_mode(void) { GPIOB->BSRR = (1UL << (PIN_RS485_DE + 16U)); }

void bsp_gpio_set_comm_mode(comm_mode_e mode)
{
    uint32_t demux_val = (uint32_t)mode & 0x03U;
    GPIOA->BSRR = ((1UL << PIN_DEMUX_A) << 16U) | ((1UL << PIN_DEMUX_B) << 16U);
    if ((demux_val & 0x01U) != 0U) { GPIOA->BSRR = (1UL << PIN_DEMUX_A); }
    if ((demux_val & 0x02U) != 0U) { GPIOA->BSRR = (1UL << PIN_DEMUX_B); }
}

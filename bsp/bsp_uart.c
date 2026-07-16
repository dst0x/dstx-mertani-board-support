#include "bsp/bsp_uart.h"
#include "bsp/bsp_gpio.h"
#include "bsp/bsp_iwdg.h"
#include "stm32g0xx.h"

#ifdef USART1_MODBUS_MODE
#include "drivers/sensor_infwin_co/infwin_co_sensor.h"

static co_ctx_s *g_co_sensor_ptr = NULL;

void bsp_modbus_set_co_sensor_ptr(co_ctx_s *ptr) {
    g_co_sensor_ptr = ptr;
}
#endif

#include <stddef.h>

#define USART1_BRR_115200   (555U)   /* 64 MHz / 115200 */
#define USART1_BRR_9600     (6667U)  /* 64 MHz / 9600   */
#define USART2_BRR_9600     (6667U)  /* 64 MHz / 9600   */

#define DEBUG_TX_MASK       (DEBUG_BUFFER_SIZE - 1U)
#define MODBUS_RX_MASK      (MODBUS_BUFFER_SIZE - 1U)
#define RS485_RX_MASK       (RS485_BUFFER_SIZE - 1U)

typedef char _debug_pow[(((DEBUG_BUFFER_SIZE) & ((DEBUG_BUFFER_SIZE) - 1U)) == 0U) ? 1 : -1];
typedef char _rs485_pow[(((RS485_BUFFER_SIZE) & ((RS485_BUFFER_SIZE) - 1U)) == 0U) ? 1 : -1];

#ifdef USART1_DEBUG_MODE
    static struct{
        uint8_t data[DEBUG_BUFFER_SIZE];
        volatile uint16_t head;
        volatile uint16_t tail;
    } debug_tx_buffer;
#endif

#ifdef USART1_MODBUS_MODE
    static struct{
        uint8_t data[MODBUS_BUFFER_SIZE];
        volatile uint16_t head;
        volatile uint16_t tail;
    } modbus_rx_buffer;
#endif

static struct {
    uint8_t data[RS485_BUFFER_SIZE];
    volatile uint16_t head;
    volatile uint16_t tail;
} rs485_rx_buffer;

static volatile uint16_t rs485_echo_count = 0U;

#ifdef USART1_DEBUG_MODE
    static uint8_t u32_to_dec(uint32_t value, uint8_t *buf){
        char temp[11U];
        uint8_t pos = 0U, len, i;
        if(value == 0){
            buf[0U] = '0';
            buf[1U] = '\0';
            return 1U;
        }
        while(value > 0U){
            temp[pos++] = (char)('0' + (uint8_t)(value % 10U));
            value /= 10U;
        }
        len = pos;
        for(i = 0U; i < len; i++){
            buf[i] = temp[len - 1U - i];
        }
        buf[len] = '\0';
        return len;
    }

    void bsp_debug_init(void){
        SET_BIT(RCC->APBENR2, RCC_APBENR2_USART1EN);
        (void)RCC->APBENR2;
        USART1->CR1 = 0U;
        USART1->BRR = USART1_BRR_115200;
        USART1->CR2 = 0U;
        USART1->CR3 = 0U;
        USART1->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
        NVIC_SetPriority(USART1_IRQn, 2U);
        NVIC_EnableIRQ(USART1_IRQn);
    }

    void bsp_debug_write_str(const char *str){
        uint16_t len = 0U;
        if(str == NULL){ return; }
        while((str[len] != '\0') && (len < (uint16_t)DEBUG_BUFFER_SIZE)){
            len++;
        }
        (void)bsp_debug_write((const uint8_t *)str, len);
    }

    void bsp_debug_write_int(const char *prefix, int32_t value, const char *suffix){
        char buff[13U];
        uint8_t pos = 0U;
        uint32_t u_val;
        bsp_debug_write_str(prefix);
        if(value < 0){
            buff[pos++] = '-';
            u_val = (uint32_t)(-(value + 1)) + 1U;
        } else {
            u_val = (uint32_t)value;
        }
        (void)u32_to_dec(u_val, &buff[pos]);
        bsp_debug_write_str(buff);
        bsp_debug_write_str(suffix);
    }

    void bsp_debug_write_fixed(const char *prefix, int32_t raw, int32_t scale, uint8_t decimal_places, const char *suffix){
        char     ibuf[12U], fbuf[5U], ftmp[4U];
        int32_t  ip, fp;
        uint32_t fa, fs;
        uint8_t  n, fi, fl;
        static const uint32_t div[4U] = {1U, 10U, 100U, 1000U};
        if ((scale == 0) || (decimal_places > 3U)) { return; }
        bsp_debug_write_str(prefix);
        ip = raw / scale;
        fp = raw % scale;
        if ((raw < 0) && (ip == 0)) { bsp_debug_write_str("-"); }
        n = u32_to_dec((ip < 0) ? (uint32_t)(-ip) : (uint32_t)ip, ibuf);
        ibuf[n] = '\0';
        if ((raw < 0) && (ip < 0)) { bsp_debug_write_str("-"); }
        bsp_debug_write_str(ibuf);
        if (decimal_places > 0U){
            bsp_debug_write_str(".");
            fa = (uint32_t)((fp < 0) ? (-fp) : fp);
            fs = div[decimal_places];
            fa = (uint32_t)((fa * fs) / (uint32_t)scale);
            fl = u32_to_dec(fa, ftmp);
            for (fi = 0U; fi < ((uint8_t)decimal_places - fl); fi++) { fbuf[fi] = '0'; }
            for (fi = 0U; fi < fl; fi++) { fbuf[(uint8_t)decimal_places - fl + fi] = ftmp[fi]; }
            fbuf[decimal_places] = '\0';
            bsp_debug_write_str(fbuf);
        }
        bsp_debug_write_str(suffix);
    }

    void bsp_debug_write_hex(const char *prefix, const uint8_t *buf, uint8_t len, const char *suffix){
        static const char hex_chars[16U] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
        char pair[3U];
        uint8_t i;
        if(buf == NULL){ return; }
        bsp_debug_write_str(prefix);
        for(i = 0U; i < len; i++){
            uint8_t byte = buf[i];
            pair[0U] = hex_chars[(byte >> 4U) & 0x0FU];
            pair[1U] = hex_chars[byte & 0x0FU];
            pair[2U] = '\0';
            bsp_debug_write_str(pair);
            if(i < (len - 1U)){
                bsp_debug_write_str(" ");
            }
        }
    }

    void bsp_usart1_irq_handler(void){
        if ((USART1->ISR & USART_ISR_TXE_TXFNF) != 0UL){
            if (debug_tx_buffer.tail != debug_tx_buffer.head){
                USART1->TDR = (uint32_t)debug_tx_buffer.data[debug_tx_buffer.tail];
                debug_tx_buffer.tail = (uint16_t)((debug_tx_buffer.tail + 1U) & DEBUG_TX_MASK);
            } else {
                CLR_BIT(USART1->CR1, USART_CR1_TXEIE_TXFNFIE);
            }
        }
    }

    uint16_t bsp_debug_write(const uint8_t *data, uint16_t len){
        uint16_t i, written = 0U;
        if(data == NULL){ return 0U; }
        for(i = 0U; i < len; i++){
            uint16_t next_head = (uint16_t)(debug_tx_buffer.head + 1U) & DEBUG_TX_MASK;
            if(next_head == debug_tx_buffer.tail){ break; }
            debug_tx_buffer.data[debug_tx_buffer.head] = data[i];
            debug_tx_buffer.head = next_head;
            written++;
        }
        if(written > 0U){
            SET_BIT(USART1->CR1, USART_CR1_TXEIE_TXFNFIE);
        }
        return written;
    }
#endif /* USART1_DEBUG_MODE */

#ifdef USART1_MODBUS_MODE
    void bsp_modbus_init(void){
        SET_BIT(RCC->APBENR2, RCC_APBENR2_USART1EN);
        (void)RCC->APBENR2;
        USART1->CR1 = 0U;
        USART1->BRR = USART1_BRR_9600;
        USART1->CR2 = 0U;
        USART1->CR3 = 0U;
        USART1->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE_RXFNEIE;
        NVIC_SetPriority(USART1_IRQn, 2U);
        NVIC_EnableIRQ(USART1_IRQn);
    }

    uint16_t bsp_modbus_write(const uint8_t *data, uint16_t len){
        uint16_t i;
        uint32_t timeout_counter = 0U;
        if(data == NULL){ return 0U; }

        bsp_gpio_rs485_drive_mode();
        for(volatile uint32_t d = 0U; d < 500U; d++) { }

        for(i = 0U; i < len; i++){
            timeout_counter = 0U;
            while((USART1->ISR & USART_ISR_TXE_TXFNF) == 0UL){
                timeout_counter++;
                if((timeout_counter & 0xFFFU) == 0U){ bsp_iwdg_refresh(); }
                if(timeout_counter > 100000U){
                    bsp_gpio_rs485_logging_mode();
                    return i;
                }
            }
            USART1->TDR = (uint32_t)data[i];
        }

        timeout_counter = 0U;
        while((USART1->ISR & USART_ISR_TC) == 0UL){
            timeout_counter++;
            if((timeout_counter & 0xFFFU) == 0U){ bsp_iwdg_refresh(); }
            if(timeout_counter > 100000U){ break; }
        }

        for(volatile uint32_t d = 0U; d < 500U; d++) { }
        bsp_gpio_rs485_logging_mode();
        for(volatile uint32_t d = 0U; d < 500U; d++) { }

        bsp_modbus_rx_flush();
        return len;
    }

    uint16_t bsp_modbus_rx_count(void){
        return (uint16_t)((modbus_rx_buffer.head - modbus_rx_buffer.tail) & MODBUS_RX_MASK);
    }

    uint8_t bsp_modbus_rx_get(void){
        uint8_t byte = 0U;
        if(modbus_rx_buffer.head != modbus_rx_buffer.tail){
            byte = modbus_rx_buffer.data[modbus_rx_buffer.tail];
            modbus_rx_buffer.tail = (uint16_t)((modbus_rx_buffer.tail + 1U) & MODBUS_RX_MASK);
        }
        return byte;
    }

    void bsp_modbus_rx_flush(void){
        modbus_rx_buffer.tail = modbus_rx_buffer.head;
    }

    void bsp_usart1_irq_handler(void){
        if ((USART1->ISR & USART_ISR_RXNE_RXFNE) != 0UL){
            uint8_t byte = (uint8_t)(USART1->RDR & 0xFFUL);
            if (g_co_sensor_ptr != NULL) {
                sensor_co_rx_byte(g_co_sensor_ptr, byte);
            }
        }
        if ((USART1->ISR & (USART_ISR_FE | USART_ISR_ORE | USART_ISR_NE)) != 0UL){
            SET_BIT(USART1->ICR, USART_ICR_FECF | USART_ICR_ORECF | USART_ICR_NECF);
        }
    }
#endif /* USART1_MODBUS_MODE */

void USART1_IRQHandler(void){
    bsp_usart1_irq_handler();
}

void bsp_rs485_init(void){
    SET_BIT(RCC->APBENR1, RCC_APBENR1_USART2EN);
    (void)RCC->APBENR1;
    USART2->CR1 = 0U;
    USART2->BRR = USART2_BRR_9600;
    USART2->CR2 = 0U;
    USART2->CR3 = 0U;
    USART2->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE_RXFNEIE;
    NVIC_SetPriority(USART2_IRQn, 1U);
    NVIC_EnableIRQ(USART2_IRQn);
}

/* BRR = 64 MHz / baudrate */
#define BSP_USART_CLK_HZ    (64000000UL)

status_e bsp_rs485_reinit(uint32_t baudrate, uint8_t parity, uint8_t stopbits)
{
    uint32_t brr;
    uint32_t cr1;
    uint32_t cr2;

    switch (baudrate) {
        case 1200U: case 2400U: case 4800U:  case 9600U:
        case 19200U: case 38400U: case 57600U: case 115200U:
            break;
        default:
            return STATUS_ERR_PARAM;
    }
    if (parity > 2U) { return STATUS_ERR_PARAM; }
    if ((stopbits < 1U) || (stopbits > 2U)) { return STATUS_ERR_PARAM; }

    /* Round to nearest integer */
    brr = (BSP_USART_CLK_HZ + (baudrate / 2U)) / baudrate;

    /* CR1 parity: PCE=0 → none; PCE=1,PS=0 → even; PCE=1,PS=1 → odd
     * M0=1 required when parity enabled (9-bit frame for 8 data + 1 parity) */
    cr1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE_RXFNEIE;
    if (parity == 1U) {
        cr1 |= USART_CR1_M0 | USART_CR1_PCE;
    } else if (parity == 2U) {
        cr1 |= USART_CR1_M0 | USART_CR1_PCE | USART_CR1_PS;
    }

    /* CR2 STOP[1:0]: 00=1 stop bit, 10=2 stop bits */
    cr2 = (stopbits == 2U) ? (0x2UL << USART_CR2_STOP_Pos) : 0U;

    CLR_BIT(USART2->CR1, USART_CR1_UE);
    USART2->BRR = brr;
    USART2->CR2 = cr2;
    USART2->CR3 = 0U;
    USART2->CR1 = cr1 | USART_CR1_UE;

    bsp_rs485_rx_flush();
    return STATUS_OK;
}

void bsp_rs485_send(const uint8_t *data, uint16_t len){
    uint16_t i;
    uint32_t timeout_counter = 0U;
    if(data == NULL){ return; }
    bsp_rs485_rx_flush();
    for(volatile uint32_t d = 0U; d < 1000U; d++){ }
    bsp_gpio_rs485_drive_mode();
    rs485_echo_count = len;
    for(volatile uint32_t d = 0U; d < 1000U; d++){ }
    for(i = 0U; i < len; i++){
        timeout_counter = 0U;
        while((USART2->ISR & USART_ISR_TXE_TXFNF) == 0UL){
            timeout_counter++;
            if((timeout_counter & 0xFFFU) == 0U){ bsp_iwdg_refresh(); }
        }
        USART2->TDR = (uint32_t)data[i];
    }
    timeout_counter = 0U;
    while((USART2->ISR & USART_ISR_TC) == 0UL){
        timeout_counter++;
        if((timeout_counter & 0xFFFU) == 0U){ bsp_iwdg_refresh(); }
    }
    for(volatile uint32_t d = 0U; d < 500U; d++){ }
    bsp_gpio_rs485_logging_mode();
    for(volatile uint32_t d = 0U; d < 500U; d++){ }
    SET_BIT(USART2->ICR, USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NECF);
    rs485_echo_count = 0U;
    bsp_rs485_rx_flush();
}

void bsp_rs485_rx_flush(void){
    rs485_rx_buffer.tail = rs485_rx_buffer.head;
}

void bsp_rs485_usart2_irq_handler(void){
    if((USART2->ISR & USART_ISR_RXNE_RXFNE) != 0UL){
        uint8_t byte = (uint8_t)(USART2->RDR & 0xFFUL);
        if(rs485_echo_count > 0U){
            rs485_echo_count--;
        } else {
            uint16_t next_head = (uint16_t)(rs485_rx_buffer.head + 1U) & RS485_RX_MASK;
            if(next_head != rs485_rx_buffer.tail){
                rs485_rx_buffer.data[rs485_rx_buffer.head] = byte;
                rs485_rx_buffer.head = next_head;
            }
        }
    }
    if((USART2->ISR & (USART_ISR_FE | USART_ISR_ORE | USART_ISR_NE)) != 0UL){
        SET_BIT(USART2->ICR, USART_ICR_FECF | USART_ICR_ORECF | USART_ICR_NECF);
    }
}

void bsp_usart2_irq_handler(void){
    bsp_rs485_usart2_irq_handler();
}

void USART2_IRQHandler(void){
    bsp_usart2_irq_handler();
}

uint16_t bsp_rs485_rx_count(void){
    return (uint16_t)((rs485_rx_buffer.head - rs485_rx_buffer.tail) & RS485_RX_MASK);
}

uint8_t bsp_rs485_rx_get(void){
    uint8_t byte = 0U;
    if(rs485_rx_buffer.tail != rs485_rx_buffer.head){
        byte = rs485_rx_buffer.data[rs485_rx_buffer.tail];
        rs485_rx_buffer.tail = (uint16_t)((rs485_rx_buffer.tail + 1U) & RS485_RX_MASK);
    }
    return byte;
}

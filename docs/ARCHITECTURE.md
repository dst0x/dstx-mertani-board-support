# Architecture Documentation - Mertani Board Support Package

## System Architecture Overview

```
┌─────────────────────────────────────────────────┐
│                 Application Layer               │
│                      (Main)                     │
└─────────────────────────────────────────────────┘
                         │
         ┌───────────────┴───────────────┐
         │                               │
┌────────▼────────┐             ┌────────▼───────┐
│ Sensor Manager  │             │  UART Manager  │
│  (Middleware)   │             │  (Middleware)  │
└───────┬─────────┘             └──────┬─────────┘
        │                              │
    ┌───┴────┐                     ┌───┴────┐
    │        │                     │        │
┌───▼───┐ ┌──▼───┐            ┌───▼───┐ ┌──▼───┐
│ SEN66 │ │  CO  │            │ USART1│ │USART2│
│Driver │ │Driver│            │  BSP  │ │ BSP  │
└───┬───┘ └──┬───┘            └───┬───┘ └──┬───┘
    │        │                    │        │
┌───▼────────▼────┐          ┌───▼────────▼────┐
│   I2C/UART BSP  │          │   Hardware HAL  │
└─────────────────┘          └─────────────────┘
         │                            │
         └────────────┬───────────────┘
                      │
              ┌───────▼────────┐
              │  STM32G0 MCU   │
              └────────────────┘
```

---

## Architecture

### Layer 1: Application Layer

```c
main.c
├─ System initialization
├─ Manager initialization
├─ Main loop
│  ├─ sensor_manager_poll()
│  ├─ modbus_service()
│  └─ LED control
└─ Watchdog refresh
```

**Responsibilities**:
- Initialize hardware (clocks, GPIO, peripherals)
- Initialize middleware managers
- Run main application loop
- Handle application-specific logic

---

### Layer 2: Middleware - Sensor Manager

```
sensor_manager.h/c
├─ Configuration
│  ├─ Sensor enable/disable
│  ├─ Timing parameters
│  ├─ Filtering options
│  └─ Error thresholds
│
├─ Management Functions
│  ├─ sensor_manager_init()
│  ├─ sensor_manager_poll()
│  ├─ sensor_manager_reset_sensor()
│  └─ sensor_manager_get_stats()
│
└─ Features
   ├─ Zero value filtering
   ├─ Periodic sensor reset
   ├─ Error recovery
   ├─ State machine per sensor
   └─ Statistics tracking
```

**Data Flow**:
```
Application
    │
    ├─ init() ────────┐
    │                 │
    └─ poll() ───┐    │
                 │    │
        ┌────────▼────▼────────┐
        │   Sensor Manager     │
        │  ┌─────────────────┐ │
        │  │ State Machine   │ │
        │  │ Error Handler   │ │
        │  │ Filter Logic    │ │
        │  └─────────────────┘ │
        └─────────┬────────────┘
                  │
          ┌───────┴────────┐
          │                │
    ┌─────▼─────┐   ┌─────▼─────┐
    │   SEN66   │   │    CO     │
    │  Driver   │   │  Driver   │
    └───────────┘   └───────────┘
```

---

### Layer 3: Middleware - UART Manager

```
uart_manager.h/c
├─ Configuration
│  ├─ USART1 mode selection
│  ├─ USART2 mode selection
│  ├─ Baudrate per mode
│  └─ Buffer sizes
│
├─ Management Functions
│  ├─ uart_manager_init()
│  ├─ uart_manager_get_mode()
│  ├─ uart_manager_set_mode()
│  └─ uart_manager_get_stats()
│
└─ Supported Modes
   ├─ RS485 (Modbus)
   ├─ RS232 (Serial)
   ├─ TTL (Debug)
   └─ SDI-12 (Future)
```

**Configuration Flow**:
```
uart_manager.h
    │
    │ #define USART1_MODE COMM_PATH_MODE_TTL
    │
    ├─ Macro Processing ────┐
    │                        │
    ├─ #define USART1_DEBUG_MODE  (derived)
    │
    └─ uart_manager_init()
            │
            ├─ Set baudrate
            ├─ Configure GPIO
            ├─ Initialize USART peripheral
            └─ Set DE/RE pins (RS485)
```

---

### Layer 4: Sensor Drivers

#### SEN66 Driver Architecture

```
sensirion_sen66.h/c
├─ Data Structure
│  ├─ sen66_ctx_s (context)
│  ├─ sen66_data_s (readings)
│  └─ sen66_state_e (state machine)
│
├─ API Functions
│  ├─ sensirion_sen66_init()
│  ├─ sensirion_sen66_start_measurement()
│  ├─ sensirion_sen66_poll()
│  ├─ sensirion_sen66_reset()
│  └─ sensirion_sen66_is_data_ready()
│
└─ Features
   ├─ I2C communication
   ├─ CRC8 checksum
   ├─ Multi-parameter reading
   └─ Data validation
```

**State Machine**:
```
    ┌─────────────┐
    │   UNINIT    │
    └──────┬──────┘
           │ init()
    ┌──────▼──────┐
    │    IDLE     │
    └──────┬──────┘
           │ start_measurement()
    ┌──────▼──────┐
    │ WARMING_UP  │◄──┐
    └──────┬──────┘   │
           │ timeout  │
    ┌──────▼──────┐   │
    │   RUNNING   │   │
    └──┬───┬──────┘   │
       │   │          │
       │   └─error────►ERROR
       │              │
       └─reset────────┘
```

#### CO Sensor Driver Architecture

```
infwin_co_sensor.h/c
├─ Data Structure
│  ├─ co_ctx_s (context)
│  ├─ co_data_s (readings)
│  └─ co_state_e (state machine)
│
├─ API Functions
│  ├─ sensor_co_init()
│  ├─ sensor_co_poll()
│  ├─ sensor_co_send_request()
│  └─ sensor_co_rx_byte()
│
└─ Features
   ├─ UART communication
   ├─ Modbus RTU protocol
   ├─ CRC16 checksum
   └─ Request/response handling
```

**Communication Flow**:
```
┌─────────────┐       Request       ┌─────────────┐
│   Driver    │────────────────────►│  CO Sensor  │
│             │                     │             │
│   ┌─────┐   │                     │             │
│   │State│   │◄────────────────────│             │
│   │ Mgr │   │      Response       └─────────────┘
│   └─────┘   │
│      │      │
│  ┌───▼───┐  │
│  │ CRC   │  │
│  │Check  │  │
│  └───┬───┘  │
│      │      │
│  ┌───▼───┐  │
│  │Parse  │  │
│  │Data   │  │
│  └───────┘  │
└─────────────┘
```

---

### Layer 5: Board Support Package (BSP)

```
BSP Layer
├─ bsp_clock.c/h      - Clock configuration (64MHz)
├─ bsp_gpio.c/h       - GPIO control (LED, DE/RE)
├─ bsp_i2c.c/h        - I2C master (for SEN66)
├─ bsp_uart.c/h       - UART driver (RS485, TTL)
├─ bsp_systick.c/h    - System tick timer (1ms)
└─ bsp_iwdg.c/h       - Watchdog timer
```

**BSP UART Module**:
```
bsp_uart.c/h
├─ USART1
│  ├─ Debug Mode (TTL, 115200 baud)
│  │  ├─ bsp_debug_init()
│  │  ├─ bsp_debug_write_str()
│  │  ├─ bsp_debug_write_int()
│  │  └─ bsp_debug_write_hex()
│  │
│  └─ Modbus Mode (RS485, 9600 baud)
│     ├─ bsp_modbus_init()
│     └─ bsp_modbus_write()
│
└─ USART2 (RS485, 9600 baud)
   ├─ bsp_rs485_init()
   ├─ bsp_rs485_send()
   └─ bsp_rs485_rx_get()
```

---

## Data Flow Diagrams

### Sensor Data Flow

```
┌─────────┐                  ┌──────────────┐
│  SEN66  │─────I2C──────────►│ bsp_i2c.c   │
│ Sensor  │                  └──────┬───────┘
└─────────┘                         │
                                    │ read_bytes()
┌─────────┐                  ┌──────▼───────┐
│   CO    │────UART──────────►│ bsp_uart.c  │
│ Sensor  │                  └──────┬───────┘
└─────────┘                         │
                                    │ rx_byte()
                             ┌──────▼───────────┐
                             │  Sensor Drivers  │
                             │                  │
                             │  ┌─────────────┐ │
                             │  │ Parse Data  │ │
                             │  │ CRC Check   │ │
                             │  │ Scale       │ │
                             │  └──────┬──────┘ │
                             └─────────┼────────┘
                                       │
                             ┌─────────▼────────┐
                             │ Sensor Manager   │
                             │                  │
                             │  ┌────────────┐  │
                             │  │ Filtering  │  │
                             │  │ Validation │  │
                             │  │ Statistics │  │
                             │  └─────┬──────┘  │
                             └────────┼─────────┘
                                      │
                             ┌────────▼────────┐
                             │  Application    │
                             │                  │
                             │ ┌─────────────┐ │
                             │ │   Modbus    │ │
                             │ │  Registers  │ │
                             │ └─────────────┘ │
                             └─────────────────┘
```

### Configuration Cascade

```
User Configuration
        │
        ├─ sensor_manager.h
        │  │
        │  ├─ #define SENSOR_ENABLE_SEN66 1
        │  ├─ #define SENSOR_POLL_INTERVAL_MS 1000
        │  └─ #define SENSOR_ENABLE_ZERO_FILTER 1
        │
        └─ uart_manager.h
           │
           ├─ #define USART1_MODE COMM_PATH_MODE_TTL
           │
           └─ Derived Macros
              │
              ├─ #define USART1_DEBUG_MODE
              ├─ #undef USART1_MODBUS_MODE
              │
              └─ bsp_uart.h (includes derived macros)
```

---

## Module Dependencies

```
main.c
 │
 ├──► sensor_manager.h
 │     │
 │     ├──► drivers/sensor_sensirion_sen66/sensirion_sen66.h
 │     │     │
 │     │     ├──► bsp/bsp_i2c.h
 │     │     └──► bsp/bsp_systick.h
 │     │
 │     └──► drivers/sensor_infwin_co/infwin_co_sensor.h
 │           │
 │           ├──► bsp/bsp_uart.h
 │           └──► middleware/modbus/modbus_crc.h
 │
 ├──► uart_manager.h
 │     │
 │     ├──► bsp/bsp_uart.h
 │     └──► bsp/bsp_gpio.h
 │
 └──► middleware/modbus/modbus_slave.h
       │
       └──► middleware/modbus/modbus_crc.h
```

---

## Error Recovery Flow

```
                  Normal Operation
                        │
                        ▼
              ┌─────────────────┐
              │   RUNNING       │
              │  Poll sensors   │
              └────┬────────┬───┘
                   │        │
         success   │        │ error detected
                   │        │
                   │   ┌────▼────────┐
                   │   │ error_count++│
                   │   └────┬─────────┘
                   │        │
                   │   ┌────▼──────────────┐
                   │   │error_count >= MAX?│
                   │   └────┬──────┬───────┘
                   │        │ YES  │ NO
                   │        │      └───────┐
                   │   ┌────▼────┐         │
                   │   │  ERROR  │         │
                   │   │  STATE  │         │
                   │   └────┬────┘         │
                   │        │              │
                   │   wait RETRY_INTERVAL │
                   │        │              │
                   │   ┌────▼────────────┐ │
                   │   │  Try reinit()   │ │
                   │   └────┬────────┬───┘ │
                   │        │ OK     │FAIL │
                   │   ┌────▼────┐   │     │
                   │   │WARMING_UP  │   │     │
                   │   └────┬────┘   │     │
                   │        │        │     │
                   │   wait warmup   │     │
                   │        │        │     │
                   └────────┴────────┴─────┘
```

---

## Memory Layout

```
Flash Memory (64KB STM32G031)
┌─────────────────────────────┐ 0x08000000
│  Vector Table               │
├─────────────────────────────┤
│  Startup Code               │
├─────────────────────────────┤
│  Application Code           │
│  ├─ main.c                  │
│  ├─ BSP drivers (~8KB)      │
│  ├─ Sensor drivers (~4KB)   │
│  ├─ Middleware (~4KB)       │
│  └─ Sensor Manager (~2KB)   │
│  └─ UART Manager (~1KB)     │
├─────────────────────────────┤
│  Constants & Strings        │
└─────────────────────────────┘ 0x08010000

RAM Memory (8KB)
┌─────────────────────────────┐ 0x20000000
│  Stack (2KB)                │
├─────────────────────────────┤
│  Heap                       │
├─────────────────────────────┤
│  Global Variables           │
│  ├─ g_aqs_sensor            │
│  ├─ g_co_sensor             │
│  ├─ g_sensor_manager        │
│  ├─ g_uart_manager          │
│  ├─ g_modbus                │
│  └─ Buffers (~1KB)          │
└─────────────────────────────┘ 0x20002000
```

---

## Timing Diagram

```
Time ───────►

Sensor Poll Cycle (1000ms):
    ├─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┤
    0    1s    2s    3s    4s    5s    6s    7s    8s    9s   10s
    │     │     │     │     │     │     │     │     │     │
    poll  poll  poll  poll  poll  poll  poll  poll  poll  poll

Error Retry Cycle (5000ms):
    │                     │                     │
    error                retry                 retry
    detected             attempt               attempt

Periodic Reset (3600000ms):
    │                                                         │
    normal operation                                      reset
    ├──────────────────────────────────────────────────────►│
    0                                                     1 hour

Watchdog Refresh:
    ├┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┤
    Every main loop iteration (~100Hz)
```

---

## Configuration Impact Matrix

| Configuration | Flash | RAM | CPU | Power |
|--------------|-------|-----|-----|-------|
| **Both sensors enabled** | +6KB | +200B | +3% | +5mA |
| **SEN66 only** | +4KB | +100B | +2% | +3mA |
| **CO only** | +2KB | +100B | +1% | +2mA |
| **Zero filter ON** | +500B | +20B | +0.5% | +0mA |
| **Periodic reset ON** | +300B | +4B | +0.1% | +0mA |
| **Debug mode ON** | +1KB | +256B | +1% | +0mA |

**Baseline**: Core system without sensors = ~10KB Flash, 1.5KB RAM

---

## Thread Safety & Concurrency

This firmware is **single-threaded** (bare metal, no RTOS), but uses:

### Interrupt-Safe Operations

```
ISR Context:
    USART1_IRQHandler()
    ├─ RX byte received
    ├─ Store in circular buffer
    └─ Set flag

Main Loop Context:
    while(1) {
        if (rx_flag) {
            process_data()  // Non-ISR context
        }
    }
```

### Critical Sections

```c
// Watchdog refresh is critical
void critical_operation(void) {
    bsp_iwdg_refresh();  // Atomic operation
    // Protected code
    bsp_iwdg_refresh();  // Ensure completion
}
```

---

## Power Consumption Modes

### Active Mode (Normal Operation)
- CPU: 64MHz
- All peripherals active
- Typical: 15-20mA @ 3.3V

### Low Power Configuration
- Reduce poll interval: 5000ms
- Disable unused sensors
- Lower UART baudrate
- Typical: 8-12mA @ 3.3V

---

## Scalability

### Adding New Sensors

1. Create driver in `drivers/sensor_new/`
2. Add to `sensor_manager.h`:
   ```c
   #define SENSOR_ENABLE_NEW 1
   ```
3. Add context to `sensor_manager_ctx_s`
4. Implement in `sensor_manager.c`
5. Update Makefile

**Estimated effort**: 2-4 hours per sensor

### Adding New Communication Modes

1. Add mode to `uart_manager.h`:
   ```c
   COMM_PATH_MODE_NEW = 4
   ```
2. Implement in `uart_manager.c`
3. Add BSP functions if needed
4. Update documentation

**Estimated effort**: 4-8 hours per protocol

---

## Version History

### v1.0 (Current)
- Centralized sensor manager
- Centralized UART manager
- Zero filtering
- Periodic reset
- Error recovery

### v0.1 (Original)
- Basic sensor support
- Fixed UART configuration
- Manual error handling

---

**Last Updated**: July 15, 2026  
**Author**: DST0x  
**Version**: 1.0

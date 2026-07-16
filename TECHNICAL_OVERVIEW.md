# GAMBARAN TEKNIS MERTANI BOARD SUPPORT PACKAGE

## 1. ARSITEKTUR LAYER SISTEM

```
┌──────────────────────────────────────────────────────┐
│                 APPLICATION LAYER                    │
│                     (main.c)                         │
│  - System initialization                             │
│  - Main loop orchestration                           │
│  - LED control                                       │
└───────────────────────┬──────────────────────────────┘
                        │
┌───────────────────────┴──────────────────────────────┐
│                 MIDDLEWARE LAYER                     │
│  ┌─────────────┬──────────────┬──────────────┐      │
│  │ Sensor Mgr  │  UART Mgr    │ Modbus Slave │      │
│  │             │              │              │      │
│  │ • Polling   │ • Mode cfg   │ • RTU proto  │      │
│  │ • Recovery  │ • RS485/232  │ • Registers  │      │
│  │ • Hotplug   │ • TTL/SDI12  │ • FC03/04/06 │      │
│  └─────────────┴──────────────┴──────────────┘      │
└───────────────────────┬──────────────────────────────┘
                        │
┌───────────────────────┴──────────────────────────────┐
│                   DRIVER LAYER                       │
│  ┌──────────┬─────────────┬──────────────┐          │
│  │  SEN66   │  Infwin CO  │   PMSX003    │          │
│  │          │             │              │          │
│  │ I2C 0x6B │ Modbus RTU  │ Serial frame │          │
│  │ PM/RH/T  │ RS485 UART1 │ UART1 passive│          │
│  └──────────┴─────────────┴──────────────┘          │
└───────────────────────┬──────────────────────────────┘
                        │
┌───────────────────────┴──────────────────────────────┐
│                     BSP LAYER                        │
│  ┌──────┬──────┬──────┬───────┬─────────┬──────┐   │
│  │Clock │ GPIO │  I2C │ UART  │ SysTick │ IWDG │   │
│  │      │      │      │       │         │      │   │
│  │64MHz │Pins  │100kHz│RS485  │ 1ms     │WDT   │   │
│  └──────┴──────┴──────┴───────┴─────────┴──────┘   │
└──────────────────────────────────────────────────────┘
```

## 2. SENSOR INFWIN CO - KONEKSI DAN CARA KERJA

### Hardware Connection
- **Interface**: UART1 (USART1)
- **Mode**: RS485 half-duplex
- **Protocol**: Modbus RTU Master
- **Baudrate**: 9600 bps, 8N1
- **Pins**: 
  - PA9: TX
  - PA10: RX
  - DE/RE: GPIO controlled via bsp_gpio

### Software Data Flow
```
Infwin CO Sensor (Slave Addr 0x01)
        │
        │ RS485 Physical Layer
        ▼
USART1_IRQHandler (bsp_uart.c)
        │
        │ Byte-by-byte routing
        ▼
sensor_co_rx_byte() (infwin_co_sensor.c)
        │
        │ Modbus frame parsing
        ▼
co_ctx_s.data (driver context)
        │
        │ Data validation
        ▼
sensor_manager_poll() (sensor_manager.c)
        │
        │ Error handling & recovery
        ▼
modbus_slave register 0x09 (modbus_slave.c)
        │
        │ Modbus RTU Slave (USART2)
        ▼
External Modbus Master
```

### Operational Flow

**1. Initialization**
```c
sensor_manager_init() 
  └─> sensor_co_init(&g_co_sensor)
       └─> Set state to IDLE
```

**2. Polling Cycle**
```c
sensor_co_poll()
  ├─> Build Modbus request: FC03, addr 0x0000, qty 1
  ├─> Send via bsp_modbus_send()
  ├─> Wait for response (timeout 500ms)
  └─> Parse response:
       ├─> Valid: extract CO value (ppm)
       ├─> CRC error: increment error count
       └─> Timeout: increment timeout count
```

**3. Error Handling**
- **Timeout count ≥ 5**: Mark as ERROR state
- **Auto-recovery**: Every 5 seconds, attempt reinit
- **Status tracking**: `sensor_manager_get_status(ctx, 1)`

### Configuration Mode
```c
// uart_manager.h
#define USART1_MODE  COMM_PATH_MODE_RS485  // For Infwin CO
```

---

## 3. SENSOR SENSIRION SEN66 - KONEKSI DAN CARA KERJA

### Hardware Connection
- **Interface**: I2C1
- **I2C Address**: 0x6B (7-bit)
- **Pins**:
  - PB6: SCL
  - PB7: SDA
- **Speed**: 100 kHz

### Software Data Flow
```
SEN66 (I2C Slave 0x6B)
        │
        │ I2C Protocol
        ▼
bsp_i2c_read() / bsp_i2c_write() (bsp_i2c.c)
        │
        │ Command sequences
        ▼
sensirion_sen66.c driver
        │
        │ Data parsing
        ▼
sen66_ctx_s.data (driver context)
        │
        │ Data fresh flag
        ▼
sensor_manager_poll() (sensor_manager.c)
        │
        │ Periodic polling (1s)
        ▼
modbus_slave registers 0x00-0x08 (modbus_slave.c)
        │
        │ Modbus RTU Slave (USART2)
        ▼
External Modbus Master
```

### Operational Flow

**1. Initialization**
```c
sensirion_sen66_init()
  ├─> Send reset command
  ├─> Wait 500ms
  ├─> Read product name
  ├─> Read serial number
  └─> Start measurement
```

**2. Data Reading**
```c
sensor_manager_poll()
  └─> Every 1 second:
       ├─> sensirion_sen66_is_data_ready()
       └─> sensirion_sen66_poll()
            ├─> Read PM values (PM1, PM2.5, PM4, PM10)
            ├─> Read RH & Temperature
            ├─> Read VOC & NOx index
            └─> Read CO2 (if available)
```

**3. Hot-plug Support**

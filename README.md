# Mertani Board Support Package v1.0
## Table of Contents
---

## 1. Architecture

```
┌─────────────────────────────────────────────────────────┐
│                   STM32G031F8P6                         │
│                                                         │
│  ┌─────────────┐   ┌──────────────┐   ┌─────────────┐   │
│  │sensor_manage│   │ uart_manager │   │modbus_slave │   │
│  │             │   │              │   │             │   │
│  │ SEN66 poll  │   │ USART1 mode  │   │ reg 0-9     │   │
│  │ CO poll     │   │ USART2 mode  │   │ reg 100-107 │   │
│  └──────┬──────┘   └──────┬───────┘   │ reg 240-243 │   │
│         │                 │           └──────┬──────┘   │
│         │                 │                  │          │
│  ┌──────▼──────┐   ┌──────▼───────┐          │          │
│  │  BSP I2C    │   │  BSP UART    │          │          │
│  │  (SEN66)    │   │  USART1/2    │          │          │
│  └──────┬──────┘   └──────┬───────┘          │          │
└─────────┼─────────────────┼──────────────────┼──────────┘
          │                 │                  │
     [PA11/PA12]     [PB6/PB7]          [PA2/PA3]
       I2C2            USART1             USART2
          │           (via MUX)              │
      SEN66           SP4T MUX           RS485 Bus
      (I2C)          /       \        (Modbus Slave)
                  RS485      TTL
                CO Sensor   Debug
```
**USART1 Communication Path Configuration:**
- RS485 Mode: Set the MUX to logic level 00 to route the UART line to the RS485 interface.
- TTL Mode: Set the MUX to logic level 11 to route the UART line to the TTL interface.
---

## 2. Directory Structure

```
mertani_board_support_v1.0/
├── main.c                          # Entry point, main loop
├── Makefile                        # Build via GNU make
├── build.ps1                       # Build via PowerShell (Windows)
├── STM32G031F8PX_FLASH.ld          # Linker script for G031
├── STM32G030F6PX_FLASH.ld          # Linker script for G030
│
├── bsp/                            # Board Support Package (hardware abstraction)
│   ├── bsp_clock.c/h               # PLL init → 64 MHz
│   ├── bsp_gpio.c/h                # GPIO, LED, MUX, RS485 DE pin
│   ├── bsp_i2c.c/h                 # I2C2 master for SEN66
│   ├── bsp_iwdg.c/h                # Independent Watchdog
│   ├── bsp_systick.c/h             # SysTick 1ms ticker
│   └── bsp_uart.c/h                # USART1 (debug/CO) + USART2 (RS485 slave)
│
├── drivers/
│   ├── sensor_sensirion_sen66/
│   │   ├── sensirion_sen66.c/h     # SEN66 driver via I2C
│   └── sensor_infwin_co/
│       ├── infwin_co_sensor.c/h    # CO sensor driver via Modbus RTU
│
├── middleware/
│   ├── sensor_manager.c/h          # *** Centralized sensor configuration ***
│   ├── uart_manager.c/h            # *** Centralized UART configuration ***
│   └── modbus/
│       ├── modbus_crc.c/h          # Modbus CRC-16
│       ├── modbus_slave.c/h        # Modbus RTU slave + register map
│
├── common/
│   └── common_types.h              # status_e, SYSCLK_HZ, common macros
│
├── CMSIS/                          # CMSIS core + ST STM32G0xx device headers
└── platform/
    └── stm32g0/
        ├── startup_stm32g030xx.c   # Startup file for G030
        └── startup_stm32g031xx.c   # Startup file for G031
```

---

## 3. Hardware Pinout

| Pin  | Function             | Description                                      |
|------|----------------------|---------------------------------------------------|
| PA0  | SP4T MUX A           | DeMux control bit 0 (see MUX table below)         |
| PA1  | SP4T MUX B           | DeMux control bit 1                               |
| PA2  | USART2 TX (AF1)      | RS485 Modbus slave TX to external master          |
| PA3  | USART2 RX (AF1)      | RS485 Modbus slave RX from external master        |
| PA4  | LED                  | Normal blink 500ms, error blink 100ms             |
| PA11 | I2C2 SCL (AF6)       | Clock line, I2C to SEN66                          |
| PA12 | I2C2 SDA (AF6)       | Data line, I2C to SEN66                           |
| PB1  | RS485 DE             | RS485 Driver Enable (HIGH=transmit, LOW=receive)  |
| PB6  | USART1 TX (AF0)      | To MUX → CO sensor or debug TTL                   |
| PB7  | USART1 RX (AF0)      | From MUX → CO sensor or debug TTL                 |

### SP4T DeMux Control (PA0/PA1)

| PA1 | PA0 | Mode    | PB6/PB7 Routing     |
|-----|-----|---------|---------------------|
|  0  |  0  | RS485   | → Modbus Sensor     |
|  1  |  1  | TTL     | → Debug serial      |

The mode is automatically controlled by firmware based on `USART1_MODE` in `uart_manager.h`.

---

## 4. UART Mode Configuration

All UART configuration is done in **a single file**: `middleware/uart_manager.h`

```c
/* Change this value to select USART1 mode */
#define USART1_MODE     COMM_PATH_MODE_RS485  /* Modbus sensor active */
// #define USART1_MODE  COMM_PATH_MODE_TTL    /* Debug serial active */
```

| `USART1_MODE`           | Effect                                                               |
|-------------------------|-----------------------------------------------------------------------|
| `COMM_PATH_MODE_RS485`  | USART1 = RS485 9600 bps, Modbus sensor active, debug OFF              |
| `COMM_PATH_MODE_TTL`    | USART1 = TTL 115200 bps, debug output active, Modbus sensor OFF       |
| `COMM_PATH_MODE_RS232`  | USART1 = RS232 9600 bps, sensor active via RS232                      |

**USART2 is always RS485** — used as a Modbus slave for the external master. Baudrate, parity, and stop bits can be changed at runtime via Modbus register 0xF1–0xF3 (see Section 6).

### Baud Rate

|   Interface  | Baud Rate | Description                    |
|--------------|-----------|---------------------------------|
| USART1 RS485 | 9600      | Communication with Modbus sensor |
| USART1 TTL   | 115200    | Debug serial output              |
| USART2 RS485 | 9600      | Modbus slave to master (configurable via register 0xF1) |

---

## 5. Sensor Configuration

All sensor configuration is done in **a single file**: `middleware/sensor_manager.h`

### Enable / Disable Sensors

```c
#define SENSOR_ENABLE_SEN66         1   /* 1=enabled, 0=disabled */
#define SENSOR_ENABLE_INFWIN_CO     1   /* 1=enabled, 0=disabled */
```

> **Note:** `SENSOR_ENABLE_INFWIN_CO = 1` only takes effect if `USART1_MODE = COMM_PATH_MODE_RS485`.
> If USART1 is in TTL mode, the CO sensor will not run even if the flag is enabled.

### Timing

```c
#define SENSOR_POLL_INTERVAL_MS     1000U    /* Normal polling interval (1 second) */
#define SENSOR_RETRY_INTERVAL_MS    5000U    /* Retry interval after an error (5 seconds) */
#define SENSOR_RESET_INTERVAL_MS    3600000U /* Periodic reset (1 hour) */
```

### Data Filtering

```c
#define SENSOR_ENABLE_ZERO_FILTER   1   /* Filter zero values (use last valid data) */
#define SENSOR_MAX_ZERO_COUNT       3   /* Max consecutive zero readings before substitution */
#define SENSOR_MAX_ERRORS           5   /* Max errors before the sensor is reset */
```

When the sensor returns a value of 0 for `SENSOR_MAX_ZERO_COUNT` consecutive readings, the system will use the last valid reading until the sensor produces valid data again.

### SEN66 — Sensirion Air Quality Sensor

| Parameter    | Value               | Description                       |
|--------------|---------------------|------------------------------------|
| Interface    | I2C2                | Address 0x6B                       |
| Clock        | 100 kHz             | Standard mode                      |
| Pins         | PA11 (SCL), PA12 (SDA)                                    |
| Data         | PM1.0, PM2.5, PM4.0, PM10, RH, Temp, VOC, NOx, CO2         |

### CO Sensor — Infwin

| Parameter     | Value                      | Description                    |
|---------------|----------------------------|----------------------------------|
| Interface     | USART1 RS485               | Modbus RTU 9600 bps              |
| Slave Address | 0x62 (98)                  | Factory default                  |
| Register      | 0x0003                     | CO value register                |
| Request frame | `62 03 00 03 00 01 [CRC]`  | 8 bytes                          |
| Response frame| `62 03 02 [CO_H] [CO_L] [CRC]` | 7 bytes                     |
| Timeout       | 1500 ms                    | Timeout waiting for response     |
| Interval      | 1000 ms                    | Request send interval            |

---

## 6. Modbus Register Map

The firmware operates as a **Modbus RTU Slave** on USART2 RS485.

- **Slave ID:** `0x01`
- **Baud Rate:** 9600 bps, 8N1
- **Function Codes:** FC03 (Read Holding Registers) and FC04 (Read Input Registers)

### Sensor Data (Addresses 0–9)

| Addr (Dec) | Addr (Hex) | Description        | Scale  | Source  | Example                       |
|-----------|-----------|----------------------|--------|---------|--------------------------------|
| 0         | 0x00      | PM1.0 µg/m³         | ÷10    | SEN66   | 573 → 57.3 µg/m³               |
| 1         | 0x01      | PM2.5 µg/m³         | ÷10    | SEN66   | 714 → 71.4 µg/m³               |
| 2         | 0x02      | PM4.0 µg/m³         | ÷10    | SEN66   | 810 → 81.0 µg/m³               |
| 3         | 0x03      | PM10  µg/m³         | ÷10    | SEN66   | 856 → 85.6 µg/m³               |
| 4         | 0x04      | Humidity %RH        | ÷100   | SEN66   | 6750 → 67.50 %RH                |
| 5         | 0x05      | Temperature °C       | ÷10    | SEN66   | 262 → 26.2 °C                    |
| 6         | 0x06      | VOC Index           | ÷10    | SEN66   | 990 → 99.0                       |
| 7         | 0x07      | NOx Index           | ÷10    | SEN66   | 10 → 1.0                         |
| 8         | 0x08      | CO2 ppm             | ×1     | SEN66   | 1815 → 1815 ppm                  |
| 9         | 0x09      | CO  ppm             | ×1     | Infwin  | 40 → 40 ppm                      |

> Registers 10–99 (0x0A–0x63): **reserved**, always return 0.

### Debug / Status (Addresses 100–107)

| Addr (Dec) | Addr (Hex) | Description             | Value                                                              |
|-----------|-----------|--------------------------|---------------------------------------------------------------------|
| 100       | 0x64      | SEN66 state             | 0=UNINIT, 1=IDLE, 2=WARMING, 3=RUNNING, 4=ERROR                     |
| 101       | 0x65      | SEN66 error count       | Number of I2C communication failures                                 |
| 102       | 0x66      | SEN66 validity flags    | Bit 0=PM valid, Bit 1=ENV valid, Bit 2=GAS valid, Bit 3=CO2 valid    |
| 103       | 0x67      | CO sensor state         | 0=UNINIT, 1=IDLE, 2=WARMING, 3=RUNNING, 4=ERROR                     |
| 104       | 0x68      | CO request count        | Total requests sent to the CO sensor                                 |
| 105       | 0x69      | CO response count       | Total valid responses received                                       |
| 106       | 0x6A      | CO timeout count        | Total timeouts (no response)                                         |
| 107       | 0x6B      | CO CRC error count      | Total CRC errors in responses                                        |

> Registers 108–239 (0x6C–0xEF): **reserved**, always return 0.

### Config / Writable (Addresses 240–243)

These registers can be **read (FC03/FC04)** to check current settings and **written (FC06)** to change them. Changes take effect immediately and are **persisted to Flash** — values survive power cycles.

| Addr (Dec) | Addr (Hex) | Description   | Valid Values                                                      | Default |
|-----------|-----------|----------------|-------------------------------------------------------------------|---------|
| 240       | 0xF0      | Slave ID       | 1 – 247                                                           | 1       |
| 241       | 0xF1      | Baudrate code  | 1=1200, 2=2400, 3=4800, **4=9600**, 5=19200, 6=38400, 7=57600, 8=115200 | 4  |
| 242       | 0xF2      | Parity         | 0=None, 1=Even, 2=Odd                                             | 0       |
| 243       | 0xF3      | Stop bits      | 1=one stop bit, 2=two stop bits                                   | 1       |

> **Note:** After writing baudrate, parity, or stop bits, the device applies the new UART settings immediately. The **response frame is sent at the old baudrate** before switching — so the master must switch its own baudrate after receiving the confirmation response.

> **Note:** After writing Slave ID, the device responds with the **new slave ID** in the response frame and uses it for all subsequent communication.

#### FC06 Write Frame Format

```
[slave_id] [0x06] [reg_hi] [reg_lo] [val_hi] [val_lo] [CRC_L] [CRC_H]
```

Minimum frame length: 8 bytes. The device echoes the request back as confirmation (standard FC06 response).

#### Config Register Examples

**Read all config registers at once:**
```
Request:  01 03 00 F0 00 04 [CRC_L] [CRC_H]
Response: 01 03 08 00 01  00 04  00 00  00 01  [CRC_L] [CRC_H]
                   -----  -----  -----  -----
                   ID=1  9600  None   1stop
```

**Change Slave ID from 1 to 5:**
```
Request:  01 06 00 F0 00 05 [CRC_L] [CRC_H]
Response: 05 06 00 F0 00 05 [CRC_L] [CRC_H]   ← response uses new ID immediately
```

**Change baudrate to 19200 (code 5):**
```
Request:  01 06 00 F1 00 05 [CRC_L] [CRC_H]
Response: 01 06 00 F1 00 05 [CRC_L] [CRC_H]   ← sent at 9600 bps (old baudrate)
          ↑ master must now switch to 19200 bps
```

**Enable Even parity:**
```
Request:  01 06 00 F2 00 01 [CRC_L] [CRC_H]
Response: 01 06 00 F2 00 01 [CRC_L] [CRC_H]
```

**Set 2 stop bits:**
```
Request:  01 06 00 F3 00 02 [CRC_L] [CRC_H]
Response: 01 06 00 F3 00 02 [CRC_L] [CRC_H]
```

#### Baudrate Code Table

| Register Value | Baudrate |
|:--------------:|----------|
| 1              | 1200 bps |
| 2              | 2400 bps |
| 3              | 4800 bps |
| **4**          | **9600 bps (default)** |
| 5              | 19200 bps |
| 6              | 38400 bps |
| 7              | 57600 bps |
| 8              | 115200 bps |

#### Flash Persistence

Config values are stored in **Flash page 31** (address `0x0800F800`, last 2 KB of 64 KB Flash). Each entry has its own magic word (`0xDEAD1234`) so entries are validated independently. If no valid config exists (e.g. first boot after firmware flash), the compile-time defaults are used.

Flash endurance for page 31 is ~10,000 erase cycles. This is sufficient for normal configuration changes but the register should not be written in a high-frequency loop.

### Example Modbus Frames

**Read all sensor data (reg 0–9):**
```
01 03 00 00 00 0A [CRC_L] [CRC_H]
```

**Read debug status (reg 100–107):**
```
01 03 00 64 00 08 [CRC_L] [CRC_H]
```

**Read all config registers (reg 240–243):**
```
01 03 00 F0 00 04 [CRC_L] [CRC_H]
```

**Sensor data response (example):**
```
01 03 14 02 3D 02 CA 03 2A 03 58 02 A3 01 06 03 F2 00 0A 07 17 00 28 [CRC_L] [CRC_H]
       PM1   PM25  PM40  PM10  RH    Temp  VOC   NOx   CO2         CO
```

---

## 7. Build and Flash

### Requirements

- [arm-none-eabi-gcc](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads) (tested with version 15.2)
- [STM32CubeProgrammer](https://www.st.com/en/development-tools/stm32cubeprog.html) for flashing (Windows)
- PowerShell 5.1+ or PowerShell Core (for `build.ps1`)
- GNU make (optional, for `Makefile`)

### Building with PowerShell (Windows)

```powershell
# Build only
.\build.ps1

# Build + flash to board
.\build.ps1 -Flash

# Clean
.\build.ps1 -Clean

# Clean + build
.\build.ps1 -Clean -Build
```

### Building with GNU Make

```bash
# Build
make

# Flash via OpenOCD
make flash

# Clean
make clean

# View memory size
make size
```

### Build Output

| File                               | Description                    |
|-------------------------------------|----------------------------------|
| `build/mertani_board_support.elf`  | ELF with debug info              |
| `build/mertani_board_support.hex`  | Intel HEX for flashing           |
| `build/mertani_board_support.bin`  | Raw binary                       |
| `build/mertani_board_support.map`  | Memory map                       |

### Compiler Configuration

| Flag           | Value              | Description                            |
|-----------------|--------------------|-------------------------------------------|
| CPU             | Cortex-M0+         | `-mcpu=cortex-m0plus -mthumb`             |
| Float ABI       | Soft               | `-mfloat-abi=soft`                        |
| Optimization    | Size               | `-Os`                                     |
| C Standard      | C11                | `-std=c11`                                |
| Sections        | Enabled            | `-ffunction-sections -fdata-sections`     |
| Define          | STM32G031xx        | `-DSTM32G031xx`                           |
| Linker script   | G031F8PX_FLASH.ld  | 64KB Flash, 8KB RAM                       |

### System Clock

```
HSI (16 MHz) → PLL (×8) → PLLR (÷2) → SYSCLK = 64 MHz
```

PLL configuration is located in `bsp/bsp_clock.c`. SysTick is configured to generate a 1 ms interrupt (1000 Hz).

---

## 8. How the System Works

### Initialization Sequence

```
bsp_systick_init()      ← Configure SysTick (must be FIRST)
bsp_clock_init()        ← Set up PLL to 64 MHz
bsp_gpio_init()         ← Configure all GPIO pins + MUX
bsp_i2c_init()          ← I2C2 for SEN66
bsp_iwdg_init()         ← Watchdog timer
uart_manager_init()     ← USART1 (CO/debug) + USART2 (RS485 slave)
modbus_slave_init()     ← Initialize Modbus context
sensor_manager_init()   ← Initialize SEN66 (3 attempts) + CO sensor
```

> **Important:** `bsp_systick_init()` must be called before `bsp_clock_init()`. If reversed, SysTick will run at the 16 MHz clock while configured for 64 MHz, causing delays 4× slower than intended.

### Main Loop

```
for (;;) {
    sensor_manager_poll()     ← Poll SEN66 every 1 second
    sensor_co_poll()          ← Poll CO sensor every 1 second (if in MODBUS mode)
    modbus_service()          ← Receive Modbus requests from master, send response
    LED blink logic           ← 500ms normal, 100ms if SEN66 error
    bsp_iwdg_refresh()        ← Reset watchdog
}
```

### Error Recovery Mechanism

**SEN66:**
1. If errors occur ≥5 times consecutively → status becomes `SENSOR_STATUS_ERROR`
2. Every 5 seconds while in error state → attempt re-initialization
3. If re-init succeeds → immediately return to `RUNNING`
4. Periodic reset every 1 hour to prevent sensor hang

**CO Sensor:**
1. Every 1 second: send a Modbus request to the sensor
2. Wait up to 1500 ms for a response
3. If timeout occurs → `timeout_count++`, retry on the next iteration
4. If a CRC error occurs → `crc_error_count++`
5. If errors occur ≥5 times → status becomes `CO_STATE_ERROR`

### Zero Filtering

When the sensor returns a value of 0:
- The `zero_count` counter increments
- If `zero_count > 3` AND valid previous data exists → use the last valid data
- If the sensor returns a non-zero value again → reset the counter and store it as the new valid data

---

## 9. Troubleshooting

### CO Sensor Not Reading (Value 0)

| Possible Cause         | Solution |
|--------------------------|-----------|
| USART1 still in TTL mode | Change `USART1_MODE` to `COMM_PATH_MODE_RS485` in `uart_manager.h` |
| RS485 A/B cables reversed | Swap the A and B cables to the CO sensor |
| Incorrect slave address  | Verify the sensor address, default is `0x62`. Change `INFWIN_CO_SENSOR_ADDR_ALT` if needed |
| Baud rate mismatch       | CO sensor default is 9600 bps. Check `COMM_RS485_BAUDRATE` |
| Check debug register 104| If `request_count = 0` → driver is not running. If `timeout_count > 0` → cable/address issue |

### SEN66 Not Reading

| Possible Cause         | Solution |
|--------------------------|-----------|
| I2C not connected        | Check PA11 (SCL) and PA12 (SDA) wiring to SEN66 |
| I2C pull-up resistors    | Add 4.7kΩ pull-ups to 3.3V on SCL and SDA |
| Sensor not ready yet     | SEN66 requires 100ms startup time before communication |
| Check debug register 101| Error count indicates the number of failed I2C communications |

### Serial Debug Output Not Showing

| Possible Cause         | Solution |
|--------------------------|-----------|
| Wrong USART1 mode        | Change `USART1_MODE` to `COMM_PATH_MODE_TTL` |
| Wrong serial monitor baud rate | Use 115200 bps, 8N1, no flow control |
| MUX not routed correctly| Ensure PA0=HIGH, PA1=HIGH (TTL mode) |

### Modbus Not Responding

| Possible Cause         | Solution |
|--------------------------|-----------|
| Incorrect Slave ID       | Default slave ID = `0x01`. Read register 0xF0 (if accessible) or check `MB_SLAVE_ID` in `modbus_slave.h`. Can be changed via FC06 write to register 0xF0 |
| Baud rate mismatch       | Default USART2 = 9600 bps 8N1. Read register 0xF1 to check current baudrate code |
| RS485 wiring             | Check A/B cables on USART2 (PA2/PA3) and the DE pin (PB1) |
| Register address out of range | Data: 0–9, Debug: 100–107, Config: 240–243. Other addresses will produce an exception |
| After baudrate change    | If register 0xF1 was written, the device switched baudrate after sending the response — update master baudrate accordingly |

### Flashing Fails

```
# Make sure the board is connected via ST-Link
# First check manually with STM32CubeProgrammer
# Then run:
.\build.ps1 -Flash
```

---

## Version History

| Version | Date        | Changes                                                                   |
|---------|-------------|----------------------------------------------------------------------------|
| 1.0     | 15 Jul 2026 | Initial release with SEN66 + CO Infwin support                             |
| 1.1     | 15 Jul 2026 | Modbus config registers 0xF0–0xF3: Slave ID, baudrate, parity, stop bits  |

---

*Mertani Board Support Package — DST0x*
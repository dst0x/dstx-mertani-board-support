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
│   ├── sensor_infwin_co/
│   │   ├── infwin_co_sensor.c/h    # CO sensor driver via Modbus RTU
│   └── sensor_pmsx003/
│       ├── pmsx003_sensor.c/h      # PMSX003 PM sensor driver via UART passive
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
| `COMM_PATH_MODE_RS485`  | USART1 = RS485 9600 bps, Modbus sensor (Infwin CO) active, debug OFF |
| `COMM_PATH_MODE_SDI12`  | USART1 = SDI-12 9600 bps, PMSX003 sensor active, debug OFF           |
| `COMM_PATH_MODE_TTL`    | USART1 = TTL 115200 bps, debug output active, sensors OFF            |
| `COMM_PATH_MODE_RS232`  | USART1 = RS232 9600 bps, sensor active via RS232                      |

**USART2 is always RS485** — used as a Modbus slave for the external master. Baudrate, parity, and stop bits can be changed at runtime via Modbus register 0xF1–0xF3 (see Section 6).

### Baud Rate

|   Interface  | Baud Rate | Description                    |
|--------------|-----------|---------------------------------|
| USART1 RS485 | 9600      | Communication with Modbus sensor (Infwin CO) |
| USART1 SDI12 | 9600      | Communication with PMSX003 sensor (passive RX) |
| USART1 TTL   | 115200    | Debug serial output              |
| USART2 RS485 | 9600      | Modbus slave to master (configurable via register 0xF1) |

---

## 5. Sensor Configuration

All sensor configuration is done in **a single file**: `middleware/sensor_manager.h`

### Enable / Disable Sensors

```c
#define SENSOR_ENABLE_SEN66         1   /* 1=enabled, 0=disabled */
#define SENSOR_ENABLE_INFWIN_CO     1   /* 1=enabled, 0=disabled */
#define SENSOR_ENABLE_PMSX003       1   /* 1=enabled, 0=disabled */
```

> **Note:** `SENSOR_ENABLE_INFWIN_CO = 1` only takes effect if `USART1_MODE = COMM_PATH_MODE_RS485`.
> `SENSOR_ENABLE_PMSX003 = 1` only takes effect if `USART1_MODE = COMM_PATH_MODE_SDI12`.
> If USART1 is in TTL mode, both sensors will not run even if their flags are enabled.

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

### 15-Poll Invalidation Logic

When a sensor is **disconnected**, the system implements a graceful data invalidation:

1. **Sensor connected**: Real-time data sent continuously
2. **Sensor disconnected**: Driver detects disconnect (5 seconds without data for PMSX003)
3. **Poll 1-14 after disconnect**: Last valid data continues to be sent
4. **Poll 15 and later**: Send zero values (0 for SEN66/CO, 0.0 float for PMSX003)
5. **Sensor reconnected**: Auto-recovery, real-time data resumes immediately

This prevents "data stuck" issues while providing a grace period for temporary disconnections.

**Example timeline** (polling every 2 seconds):
```
Time  | Sensor State | Poll# | Data Sent
------|--------------|-------|------------
0s    | Connected    | 1     | 78.0 (real-time)
2s    | Connected    | 2     | 102.0 (real-time)
4s    | DISCONNECTED | 3     | 102.0 (last valid)
6s    | Disconnected | 4     | 102.0 (last valid)
...   | ...          | ...   | ...
30s   | Disconnected | 15    | 102.0 (last valid)
32s   | Disconnected | 16    | 0.0 (invalidated)
34s   | Disconnected | 17    | 0.0
...   | ...          | ...   | ...
50s   | RECONNECTED  | 20    | 95.0 (real-time)
```

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

### PMSX003 — Plantower Particulate Matter Sensor

| Parameter     | Value                      | Description                    |
|---------------|----------------------------|----------------------------------|
| Interface     | USART1 SDI-12 (9600 bps)   | Passive serial stream            |
| Protocol      | Proprietary 32-byte frame  | Start: 0x42 0x4D                 |
| Frame rate    | ~1 Hz                      | Continuous streaming             |
| Data          | PM1.0, PM2.5, PM10         | Atmospheric environment values   |
| Pins          | PB7 (RX only)              | TX not used (passive mode)       |
| Timeout       | 3 seconds                  | Frame reception timeout          |
| Disconnect    | 5 seconds                  | No frames → ERROR state          |

> **Note:** PMSX003 and Infwin CO **cannot be used simultaneously** — both require USART1. Set `USART1_MODE` to either:
> - `COMM_PATH_MODE_RS485` for Infwin CO (Modbus)
> - `COMM_PATH_MODE_SDI12` for PMSX003 (passive serial)

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

### PMSX003 Data (Addresses 1040–1045, IEEE 754 Float DCBA)

| Addr (Dec) | Addr (Hex) | Description        | Format            | Source   | Example                       |
|-----------|-----------|----------------------|-------------------|----------|--------------------------------|
| 1040      | 0x0410    | PM1.0 LSW           | IEEE 754 (bytes BA) | PMSX003  | \[0x0000\] \[0x4248\] = 50.0  |
| 1041      | 0x0411    | PM1.0 MSW           | IEEE 754 (bytes DC) | PMSX003  |                                |
| 1042      | 0x0412    | PM2.5 LSW           | IEEE 754 (bytes BA) | PMSX003  | \[0x0000\] \[0x42CC\] = 102.0 |
| 1043      | 0x0413    | PM2.5 MSW           | IEEE 754 (bytes DC) | PMSX003  |                                |
| 1044      | 0x0414    | PM10 LSW            | IEEE 754 (bytes BA) | PMSX003  | \[0x0000\] \[0x42F4\] = 122.0 |
| 1045      | 0x0415    | PM10 MSW            | IEEE 754 (bytes DC) | PMSX003  |                                |

**IEEE 754 DCBA Byte Order** (little-endian):
- Float value stored in 2 consecutive registers (4 bytes total)
- Byte order: `[B A] [D C]` where `[A B C D]` is the IEEE 754 representation (A=LSB, D=MSB)
- LSW (Least Significant Word) = bytes B A (first register)
- MSW (Most Significant Word) = bytes D C (second register)

**Decoding example:**
```
Register 0x0410 = 0x0000  (bytes B A)
Register 0x0411 = 0x4248  (bytes D C)
→ Bytes in memory: [00 00 48 42]
→ IEEE 754 float = 50.0 µg/m³
```

**Comparison of byte orders:**
```
ABCD (big-endian):    [0x42480000] stored as [0x4248][0x0000]
DCBA (little-endian): [0x42480000] stored as [0x0000][0x4248]  ← THIS FORMAT
CDAB (middle-endian): [0x42480000] stored as [0x4248][0x0000]
```

> **Note:** PMSX003 data is only available when `USART1_MODE = COMM_PATH_MODE_SDI12`.

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
| 108       | 0x6C      | PMSX003 state           | 0=UNINIT, 1=IDLE, 2=RECEIVING, 3=READY, 4=ERROR                     |
| 109       | 0x6D      | PMSX003 error count     | Total frame errors (checksum, timeout)                               |

> Registers 110–239 (0x6E–0xEF): **reserved**, always return 0.

### Config / Writable (Addresses 240–247)

These registers can be **read (FC03/FC04)** to check current settings and **written (FC06)** to change them. Changes take effect immediately and are **persisted to Flash** — values survive power cycles.

| Addr (Dec) | Addr (Hex) | Description   | Valid Values                                                      | Default |
|-----------|-----------|----------------|-------------------------------------------------------------------|---------|
| 240       | 0xF0      | Slave ID       | 1 – 247                                                           | 1       |
| 241       | 0xF1      | Baudrate code  | 1=1200, 2=2400, 3=4800, **4=9600**, 5=19200, 6=38400, 7=57600, 8=115200 | 4  |
| 242       | 0xF2      | Parity         | 0=None, 1=Even, 2=Odd                                             | 0       |
| 243       | 0xF3      | Stop bits      | 1=one stop bit, 2=two stop bits                                   | 1       |
| 244       | 0xF4      | Last update    | seconds since boot                                                | Read-only |
| 245       | 0xF5      | PMSX003 PM1.0 calibration | 100 – 10000 (fixed-point ×1000)                      | **160** (0.16x) |
| 246       | 0xF6      | PMSX003 PM2.5 calibration | 100 – 10000 (fixed-point ×1000)                      | **165** (0.165x) |
| 247       | 0xF7      | PMSX003 PM10 calibration  | 100 – 10000 (fixed-point ×1000)                      | **180** (0.18x) |

**Note on PMSX003 Calibration Defaults:**
The default calibration values (160, 165, 180) were empirically determined by comparing PMSX003 readings with Sensirion SEN66 reference sensor under identical conditions. These values provide approximately ±1% accuracy match to SEN66. You can adjust these values via Modbus FC06 if your PMSX003 unit has different characteristics.

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

#### PMSX003 Calibration Factors

Calibration factors allow you to adjust PMSX003 readings to match a reference sensor (e.g., SEN66).

**Format**: Fixed-point ×1000
- Value 1000 = 1.0x (no correction)
- Value 1500 = 1.5x (multiply by 1.5)
- Value 500 = 0.5x (divide by 2)
- Range: 100 (0.1x) to 10000 (10.0x)

**Example Calibration Process:**

1. **Collect Data** from both sensors in same conditions:
   ```
   SEN66:    PM2.5 = 9.5 µg/m³,  PM10 = 15.0 µg/m³
   PMSX003:  PM2.5 = 58.0 µg/m³, PM10 = 83.0 µg/m³
   ```

2. **Calculate Correction Factors:**
   ```
   PM2.5 factor = 9.5 / 58.0 = 0.164 → 164 (in ×1000 format)
   PM10 factor  = 15.0 / 83.0 = 0.181 → 181 (in ×1000 format)
   ```

3. **Write Calibration via Modbus FC06:**
   ```
   # Set PM1.0 calibration = 160
   Request:  01 06 00 F5 00 A0 [CRC_L] [CRC_H]
   
   # Set PM2.5 calibration = 164
   Request:  01 06 00 F6 00 A4 [CRC_L] [CRC_H]
   
   # Set PM10 calibration = 181
   Request:  01 06 00 F7 00 B5 [CRC_L] [CRC_H]
   ```

4. **Verify Changes:**
   ```
   Request:  01 03 00 F5 00 03 [CRC_L] [CRC_H]
   Response: 01 03 06 00 A0 00 A4 00 B5 [CRC_L] [CRC_H]
                      160   164   181
   ```

5. **Test Corrected Values:**
   ```
   PMSX003 Raw: 58.0 µg/m³ × 0.164 = 9.51 µg/m³ ✅ (matches SEN66)
   PMSX003 Raw: 83.0 µg/m³ × 0.181 = 15.02 µg/m³ ✅ (matches SEN66)
   ```

**Read All Calibration Registers:**
```
Request:  01 03 00 F5 00 03 [CRC_L] [CRC_H]
Response: 01 03 06 [PM1.0_H] [PM1.0_L] [PM2.5_H] [PM2.5_L] [PM10_H] [PM10_L] [CRC]
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

**Read PMSX003 data (reg 1040–1045, 6 registers = 3 float values):**
```
01 03 04 10 00 06 [CRC_L] [CRC_H]
```

**Read debug status (reg 100–109):**
```
01 03 00 64 00 0A [CRC_L] [CRC_H]
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

**PMSX003 data response (example - IEEE 754 floats):**
```
01 03 0C 42 48 00 00 42 CC 00 00 42 F4 00 00 [CRC_L] [CRC_H]
       PM1.0=50.0    PM2.5=102.0   PM10=122.0
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

### PMSX003 Not Reading (Value 0)

| Possible Cause         | Solution |
|--------------------------|-----------|
| USART1 in wrong mode     | Change `USART1_MODE` to `COMM_PATH_MODE_SDI12` in `uart_manager.h` |
| TX/RX swapped            | PMSX003 TX → MCU RX (PB7). PMSX003 RX not connected |
| Wrong baudrate           | PMSX003 default is 9600 bps passive mode |
| Check debug register 108 | If state = ERROR → checksum or timeout issue |
| Check debug register 109 | If error_count > 0 → frame reception problems |
| Sensor not powered       | PMSX003 requires 5V power supply |

### PMSX003 Shows Last Data After Disconnect

This is **expected behavior** for the first 14 Modbus polls after disconnect:
1. Sensor disconnected → driver detects after 5 seconds
2. Poll 1-14: Last valid data sent (grace period)
3. Poll 15+: Zero values sent (0.0 float)

Check register 0x6C (state):
- `3 (READY)` = sensor connected
- `4 (ERROR)` = sensor disconnected

### PMSX003 vs Infwin CO Conflict

| Problem                  | Solution |
|--------------------------|-----------|
| Both sensors need USART1 | Choose one: set `USART1_MODE` to either `COMM_PATH_MODE_RS485` (CO) or `COMM_PATH_MODE_SDI12` (PMSX003) |
| Want both sensors        | Add external UART (e.g., LPUART or USB-to-UART adapter), or use time-division multiplexing (not recommended) |

---

## Version History

| Version | Date        | Changes                                                                   |
|---------|-------------|----------------------------------------------------------------------------|
| 1.0     | 15 Jul 2026 | Initial release with SEN66 + CO Infwin support                             |
| 1.1     | 15 Jul 2026 | Modbus config registers 0xF0–0xF3: Slave ID, baudrate, parity, stop bits  |
| 1.2     | 15 Jul 2026 | Added PMSX003 sensor support with IEEE 754 float DCBA format (little-endian) |
| 1.3     | 15 Jul 2026 | Implemented 15-poll invalidation logic for graceful disconnect handling    |
| 1.4     | 15 Jul 2026 | **Fixed** SDI12 mode baudrate: 1200→9600 bps for PMSX003 compatibility    |
| 1.5     | 15 Jul 2026 | **Fixed** IWDG timeout: 2s→8s + auto-refresh during long delays           |
| 1.6     | 15 Jul 2026 | **Fixed** RS485 timing: Added proper settling delays for Modbus reliability |
| 1.7     | 15 Jul 2026 | **Added** Custom 3-byte header (0x00 0x00 0x48) for FC03/FC04 Read responses |

---

*Mertani Board Support Package — DST0x*
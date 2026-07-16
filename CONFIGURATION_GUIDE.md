# Configuration Guide - Mertani Board Support Package

## Table of Contents
1. [Sensor Configuration](#sensor-configuration)
2. [UART/Communication Configuration](#uart-communication-configuration)
3. [Advanced Settings](#advanced-settings)
4. [Common Configuration Scenarios](#common-configuration-scenarios)

---

## Sensor Configuration

### File: `middleware/sensor_manager.h`

### 1. Enable/Disable Sensors

```c
/* ========================================================================
 * SENSOR SELECTION - Configure which sensors are active
 * ======================================================================== */
#define SENSOR_ENABLE_SEN66         1   /* 1=Enable, 0=Disable */
#define SENSOR_ENABLE_INFWIN_CO     1   /* 1=Enable, 0=Disable */
```

**Notes**:
- Set to `1` to enable a sensor
- Set to `0` to disable a sensor (saves Flash and RAM)
- Disabled sensors are not compiled into the firmware

### 2. Timing Configuration

```c
/* ========================================================================
 * SENSOR TIMING CONFIGURATION
 * ======================================================================== */
#define SENSOR_POLL_INTERVAL_MS     1000U   /* Normal polling interval */
#define SENSOR_RETRY_INTERVAL_MS    5000U   /* Retry interval after error */
#define SENSOR_RESET_INTERVAL_MS    3600000U /* Periodic reset (1 hour) */
```

**SENSOR_POLL_INTERVAL_MS**:
- How often to read sensor data in normal operation
- Recommended: 1000ms (1 second) for most applications
- Lower values = more frequent updates but higher CPU usage
- Higher values = less frequent updates but lower power consumption

**SENSOR_RETRY_INTERVAL_MS**:
- How long to wait before retrying after sensor failure
- Recommended: 5000ms (5 seconds)
- Too short: may waste CPU on failing sensor
- Too long: slow recovery from temporary failures

**SENSOR_RESET_INTERVAL_MS**:
- How often to perform preventive sensor reset
- Recommended: 3600000ms (1 hour)
- Purpose: prevents sensor hang during long operation
- Set to 0 to disable periodic reset

### 3. Data Filtering Configuration

```c
/* ========================================================================
 * SENSOR FILTERING CONFIGURATION
 * ======================================================================== */
#define SENSOR_ENABLE_ZERO_FILTER   1       /* Filter out zero readings */
#define SENSOR_MAX_ZERO_COUNT       3       /* Max consecutive zeros before using last valid */
#define SENSOR_MAX_ERRORS           5       /* Max errors before sensor reset */
```

**SENSOR_ENABLE_ZERO_FILTER**:
- `1` = Enable zero filtering (recommended)
- `0` = Disable, use raw sensor values
- When enabled: system uses last valid reading if sensor repeatedly returns zero

**SENSOR_MAX_ZERO_COUNT**:
- How many consecutive zero readings to allow before filtering
- Recommended: 3
- Example behavior:
  - 1st zero → use zero value
  - 2nd zero → use zero value
  - 3rd zero → use zero value
  - 4th zero → use last valid non-zero value
  - Valid reading → reset counter, update last valid value

**SENSOR_MAX_ERRORS**:
- Maximum communication errors before marking sensor as failed
- Recommended: 5
- Lower values = faster error detection but more false alarms
- Higher values = more tolerant but slower error detection

### 4. Warmup Times

```c
/* ========================================================================
 * SENSOR WARMUP TIMES
 * ======================================================================== */
#define SENSOR_SEN66_WARMUP_MS      60000U  /* SEN66 warmup time */
#define SENSOR_CO_WARMUP_MS         30000U  /* CO sensor warmup time */
```

**Purpose**:
- Sensors need time to stabilize after power-on or reset
- During warmup, data may be invalid
- System waits for warmup before marking sensor as "RUNNING"

**SEN66_WARMUP_MS**:
- Recommended: 60000ms (60 seconds)
- Datasheet specifies 60 second warmup for accurate readings

**CO_WARMUP_MS**:
- Recommended: 30000ms (30 seconds)
- Adjust based on your CO sensor model

---

## UART Communication Configuration

### File: `middleware/uart_manager.h`

### 1. Select Communication Mode

```c
/* ========================================================================
 * USART1 CONFIGURATION - Select one mode
 * ======================================================================== */
#define USART1_MODE    COMM_PATH_MODE_TTL  /* Change this to select USART1 mode */
```

**Available Modes**:

| Mode | Value | Description | Use Case |
|------|-------|-------------|----------|
| TTL | `COMM_PATH_MODE_TTL` | TTL serial (3.3V logic) | Debug output, development |
| RS485 | `COMM_PATH_MODE_RS485` | RS485 differential | Modbus, long distance |
| RS232 | `COMM_PATH_MODE_RS232` | RS232 serial | Legacy serial devices |
| SDI-12 | `COMM_PATH_MODE_SDI12` | SDI-12 protocol | Scientific sensors |

**How to Select**:
```c
// For debug output during development:
#define USART1_MODE    COMM_PATH_MODE_TTL

// For Modbus communication:
#define USART1_MODE    COMM_PATH_MODE_RS485

// For RS232 devices:
#define USART1_MODE    COMM_PATH_MODE_RS232

// For SDI-12 sensors:
#define USART1_MODE    COMM_PATH_MODE_SDI12
```

### 2. USART2 Configuration

```c
/* ========================================================================
 * USART2 CONFIGURATION - Always RS485 for Modbus
 * ======================================================================== */
#define USART2_MODE    COMM_PATH_MODE_RS485
```

**Note**: USART2 is typically dedicated to RS485/Modbus communication.

### 3. Baudrate Configuration

```c
/* ========================================================================
 * COMMUNICATION PATH PARAMETERS
 * ======================================================================== */
#define COMM_RS485_BAUDRATE     9600U
#define COMM_RS232_BAUDRATE     9600U
#define COMM_TTL_BAUDRATE       115200U
#define COMM_SDI12_BAUDRATE     1200U
```

**Standard Baudrates**:
- **TTL Debug**: 115200 bps (fast for debug output)
- **RS485 Modbus**: 9600 bps (standard for Modbus RTU)
- **RS232**: 9600 bps (common for industrial devices)
- **SDI-12**: 1200 bps (SDI-12 protocol specification)

**Custom Baudrates**:
```c
// For high-speed RS485 (if supported by devices):
#define COMM_RS485_BAUDRATE     57600U

// For slow serial devices:
#define COMM_RS232_BAUDRATE     4800U
```

### 4. RS485 Timing

```c
#define COMM_RS485_DE_ASSERT_DELAY_US   50U     /* DE pin timing */
#define COMM_RS485_DE_NEGATE_DELAY_US   50U
```

**Purpose**:
- RS485 needs time to switch between transmit and receive
- DE (Driver Enable) pin control timing

**Typical Values**:
- 50µs is safe for most RS485 transceivers
- Check your RS485 chip datasheet for optimal values

---

## Advanced Settings

### Memory Configuration

In `uart_manager.h`:

```c
/* ========================================================================
 * BUFFER SIZES
 * ======================================================================== */
#define UART_TX_BUFFER_SIZE     256U
#define UART_RX_BUFFER_SIZE     256U
#define MODBUS_BUFFER_SIZE      64U
```

**Guidelines**:
- Larger buffers = more RAM usage but less risk of overflow
- Smaller buffers = less RAM but may lose data under heavy load
- Modbus typically needs 64 bytes (max frame size)

### Debug vs. Production Build

**Debug Build** (for development):
```c
// sensor_manager.h
#define SENSOR_ENABLE_ZERO_FILTER   1   // Keep filtering
#define SENSOR_POLL_INTERVAL_MS     1000U  // Normal speed

// uart_manager.h
#define USART1_MODE    COMM_PATH_MODE_TTL  // Debug output
```

**Production Build** (for deployment):
```c
// sensor_manager.h
#define SENSOR_ENABLE_ZERO_FILTER   1   // Keep filtering
#define SENSOR_POLL_INTERVAL_MS     1000U  // Normal speed

// uart_manager.h
#define USART1_MODE    COMM_PATH_MODE_RS485  // Modbus
```

---

## Common Configuration Scenarios

### Scenario 1: Development & Testing

**Goal**: Debug output, single sensor testing

```c
// sensor_manager.h
#define SENSOR_ENABLE_SEN66         1
#define SENSOR_ENABLE_INFWIN_CO     0   // Disable CO sensor
#define SENSOR_POLL_INTERVAL_MS     1000U

// uart_manager.h
#define USART1_MODE    COMM_PATH_MODE_TTL  // Debug output
```

**Result**:
- SEN66 sensor active
- Debug messages on USART1 (115200 bps)
- Can monitor sensor readings in terminal

### Scenario 2: Production Modbus Gateway

**Goal**: Both sensors, Modbus communication, no debug output

```c
// sensor_manager.h
#define SENSOR_ENABLE_SEN66         1
#define SENSOR_ENABLE_INFWIN_CO     1
#define SENSOR_POLL_INTERVAL_MS     1000U
#define SENSOR_ENABLE_ZERO_FILTER   1

// uart_manager.h
#define USART1_MODE    COMM_PATH_MODE_RS485  // For CO sensor
#define USART2_MODE    COMM_PATH_MODE_RS485  // For Modbus master
```

**Result**:
- Both sensors active
- CO sensor on USART1 (RS485)
- Modbus on USART2 (RS485)
- No debug output

### Scenario 3: Low Power Operation

**Goal**: Minimize power consumption

```c
// sensor_manager.h
#define SENSOR_ENABLE_SEN66         1
#define SENSOR_ENABLE_INFWIN_CO     0   // Disable if not needed
#define SENSOR_POLL_INTERVAL_MS     5000U  // Poll every 5 seconds
#define SENSOR_RESET_INTERVAL_MS    7200000U // Reset every 2 hours

// uart_manager.h
#define USART1_MODE    COMM_PATH_MODE_RS485
#define COMM_RS485_BAUDRATE     9600U  // Slower baudrate
```

**Result**:
- Less frequent sensor polling
- Lower CPU usage
- Extended battery life

### Scenario 4: High Reliability Application

**Goal**: Maximum stability, quick error recovery

```c
// sensor_manager.h
#define SENSOR_ENABLE_SEN66         1
#define SENSOR_ENABLE_INFWIN_CO     1
#define SENSOR_POLL_INTERVAL_MS     1000U
#define SENSOR_RETRY_INTERVAL_MS    3000U  // Faster retry
#define SENSOR_RESET_INTERVAL_MS    1800000U // Reset every 30 min
#define SENSOR_ENABLE_ZERO_FILTER   1
#define SENSOR_MAX_ZERO_COUNT       5      // More tolerant
#define SENSOR_MAX_ERRORS           3      // Faster error detection
```

**Result**:
- Aggressive error detection and recovery
- Frequent preventive resets
- Zero filtering for data continuity

### Scenario 5: Minimal Firmware (Single Sensor)

**Goal**: Smallest firmware size

```c
// sensor_manager.h
#define SENSOR_ENABLE_SEN66         1
#define SENSOR_ENABLE_INFWIN_CO     0   // Disabled, not compiled
#define SENSOR_ENABLE_ZERO_FILTER   0   // Disable filtering
#define SENSOR_RESET_INTERVAL_MS    0   // Disable periodic reset

// uart_manager.h
#define USART1_MODE    COMM_PATH_MODE_RS485
```

**Result**:
- Smallest Flash footprint
- Only essential features
- CO sensor driver not included in build

---

## Configuration Checklist

Before building firmware:

- [ ] Selected correct sensors in `sensor_manager.h`
- [ ] Configured appropriate timing intervals
- [ ] Enabled/disabled zero filtering as needed
- [ ] Selected correct USART1 mode in `uart_manager.h`
- [ ] Set correct baudrates for your application
- [ ] Verified sensor warmup times match your sensors
- [ ] Checked buffer sizes are adequate
- [ ] Decided on periodic reset interval

After building:

- [ ] Verify firmware size fits in Flash
- [ ] Test sensor initialization
- [ ] Verify communication on selected UART
- [ ] Monitor sensor readings for validity
- [ ] Check error recovery behavior

---

## Tips & Best Practices

### 1. Start Simple
Begin with one sensor and TTL debug mode. Add complexity gradually.

### 2. Use Debug Mode
Always develop with `USART1_MODE = COMM_PATH_MODE_TTL` to see what's happening.

### 3. Test Error Recovery
Disconnect sensor power during operation to verify recovery works.

### 4. Monitor Zero Filtering
If you see frequent zeros, investigate sensor wiring or power supply.

### 5. Tune Timing for Your Application
- Fast response needed → lower SENSOR_POLL_INTERVAL_MS
- Low power needed → higher SENSOR_POLL_INTERVAL_MS

### 6. Document Your Configuration
Add comments in the configuration files explaining your choices.

### 7. Version Control
Keep different configurations for development vs. production builds.

---

## Troubleshooting Configuration Issues

### Problem: Sensor not initializing

**Check**:
1. Is sensor enabled in `sensor_manager.h`?
2. Is I2C/UART properly initialized?
3. Is power supply adequate?

### Problem: No debug output

**Check**:
1. Is `USART1_MODE = COMM_PATH_MODE_TTL`?
2. Is baudrate correct (115200)?
3. Are TX/RX pins connected correctly?

### Problem: Frequent sensor errors

**Check**:
1. Increase `SENSOR_MAX_ERRORS`
2. Increase `SENSOR_RETRY_INTERVAL_MS`
3. Check sensor power supply stability
4. Check I2C pull-up resistors

### Problem: Firmware too large

**Solution**:
1. Disable unused sensors
2. Disable zero filtering if not needed
3. Reduce buffer sizes
4. Use `-Os` optimization (already in Makefile)

---

## Quick Reference Card

### Sensor Manager Constants

| Setting | Recommended | Range | Purpose |
|---------|-------------|-------|---------|
| POLL_INTERVAL | 1000ms | 100-10000ms | Reading frequency |
| RETRY_INTERVAL | 5000ms | 1000-30000ms | Error retry delay |
| RESET_INTERVAL | 3600000ms | 0-86400000ms | Preventive reset |
| MAX_ZERO_COUNT | 3 | 1-10 | Zero filter threshold |
| MAX_ERRORS | 5 | 1-20 | Error tolerance |

### UART Modes Quick Reference

| Mode | Baudrate | Use Case |
|------|----------|----------|
| TTL | 115200 | Debug/Development |
| RS485 | 9600 | Modbus/Industrial |
| RS232 | 9600 | Legacy Serial |
| SDI-12 | 1200 | Scientific Sensors |

---

**Last Updated**: July 15, 2026  
**Version**: 1.0

# Quick Start Guide - Mertani Board Support Package

## 5-Minute Setup

### Step 1: Choose Your Configuration (30 seconds)

Open `middleware/sensor_manager.h` and configure:

```c
// ✅ Enable sensors you want to use
#define SENSOR_ENABLE_SEN66         1    // 1=ON, 0=OFF
#define SENSOR_ENABLE_INFWIN_CO     1    // 1=ON, 0=OFF

// ✅ Enable data filtering (recommended)
#define SENSOR_ENABLE_ZERO_FILTER   1    // Filters zero readings
```

Open `middleware/uart_manager.h` and configure:

```c
// ✅ Select USART1 mode (choose ONE)
#define USART1_MODE    COMM_PATH_MODE_TTL     // For debug
// #define USART1_MODE COMM_PATH_MODE_RS485   // For production
```

### Step 2: Build (1 minute)

```bash
cd mertani_board_support_v0.1
make clean
make
```

Expected output:
```
  CC   main.c
  CC   bsp/bsp_clock.c
  CC   middleware/sensor_manager.c
  CC   middleware/uart_manager.c
  ...
  LD   build/mertani_board_support.elf
  HEX  build/mertani_board_support.hex
  BIN  build/mertani_board_support.bin
```

### Step 3: Flash (30 seconds)

```bash
make flash
```

Or use your preferred flash tool.

### Step 4: Test (3 minutes)

**With Debug Mode (TTL)**:
1. Connect UART (115200 baud, 8N1)
2. Open terminal
3. Power on board
4. You should see:
   ```
   =================================
   MERTANI BSP V1.0
   UART MANAGER - TTL DEBUG MODE
   =================================
   [UART_MGR] USART1: TTL @ 115200 bps
   [SENSOR_MGR] SEN66 warming up...
   [SEN66] PM2.5:25.3 | Temp:24.5C | ...
   ```

**With Production Mode (RS485)**:
1. Connect Modbus master
2. Query registers (address 0x01)
3. Read sensor data

---

## Common Configurations

### 🔧 Configuration A: Development/Debug

**Use Case**: Testing, development, debugging

```c
// sensor_manager.h
#define SENSOR_ENABLE_SEN66         1
#define SENSOR_ENABLE_INFWIN_CO     0
#define SENSOR_ENABLE_ZERO_FILTER   1

// uart_manager.h
#define USART1_MODE    COMM_PATH_MODE_TTL
```

**What you get**:
- ✅ Debug output on USART1
- ✅ SEN66 sensor active
- ✅ Real-time sensor data in terminal
- ✅ Diagnostic messages

### 🏭 Configuration B: Production Modbus

**Use Case**: Deployed system, Modbus communication

```c
// sensor_manager.h
#define SENSOR_ENABLE_SEN66         1
#define SENSOR_ENABLE_INFWIN_CO     1
#define SENSOR_ENABLE_ZERO_FILTER   1

// uart_manager.h
#define USART1_MODE    COMM_PATH_MODE_RS485
```

**What you get**:
- ✅ Both sensors active
- ✅ RS485 Modbus communication
- ✅ No debug output (smaller firmware)
- ✅ Industrial protocol support

### 🔋 Configuration C: Low Power

**Use Case**: Battery-powered, energy-efficient

```c
// sensor_manager.h
#define SENSOR_ENABLE_SEN66         1
#define SENSOR_ENABLE_INFWIN_CO     0
#define SENSOR_POLL_INTERVAL_MS     5000U  // 5 seconds
#define SENSOR_ENABLE_ZERO_FILTER   0      // Disable to save CPU

// uart_manager.h
#define USART1_MODE    COMM_PATH_MODE_RS485
#define COMM_RS485_BAUDRATE     9600U
```

**What you get**:
- ✅ Lower power consumption
- ✅ Longer battery life
- ✅ Slower sensor updates
- ✅ Minimal CPU usage

---

## Troubleshooting

### ❌ Problem: Build fails with "sensor_manager.h not found"

**Solution**:
```bash
# Verify files exist
ls middleware/sensor_manager.h
ls middleware/uart_manager.h

# Clean and rebuild
make clean
make
```

### ❌ Problem: No debug output on UART

**Checklist**:
- [ ] `USART1_MODE` set to `COMM_PATH_MODE_TTL`?
- [ ] Terminal configured to 115200 baud, 8N1?
- [ ] TX pin connected to RX of USB-UART adapter?
- [ ] Ground connected?

**Quick test**:
```bash
# On Linux/Mac
screen /dev/ttyUSB0 115200

# On Windows
# Use PuTTY or TeraTerm
```

### ❌ Problem: Sensor not detected

**Checklist**:
- [ ] Sensor enabled in `sensor_manager.h`?
- [ ] I2C pull-up resistors present (2.2kΩ - 4.7kΩ)?
- [ ] Sensor powered (3.3V)?
- [ ] I2C address correct (SEN66 = 0x6B)?

**Debug steps**:
1. Enable debug mode
2. Check initialization messages
3. Verify I2C communication with logic analyzer

### ❌ Problem: Frequent sensor errors

**Solution**:
```c
// In sensor_manager.h
#define SENSOR_MAX_ERRORS           10  // Increase tolerance
#define SENSOR_RETRY_INTERVAL_MS    3000U  // Faster retry
```

---

## Command Reference

### Build Commands

```bash
# Clean build artifacts
make clean

# Build firmware
make

# Flash to MCU
make flash

# Generate disassembly
make dump

# Show memory usage
make size
```

### Flash Commands (Manual)

```bash
# Using OpenOCD
openocd -f interface/stlink.cfg -f target/stm32g0x.cfg \
  -c "program build/mertani_board_support.bin verify reset exit 0x08000000"

# Using STM32CubeProgrammer
STM32_Programmer_CLI -c port=SWD -w build/mertani_board_support.hex -v -rst
```

---

## File Locations

### 📝 Configuration Files (Edit These)
- `middleware/sensor_manager.h` - Sensor configuration
- `middleware/uart_manager.h` - UART configuration

### 📖 Documentation Files (Read These)
- `README.md` - Project overview
- `CONFIGURATION_GUIDE.md` - Detailed configuration help
- `ARCHITECTURE.md` - System architecture
- `IMPLEMENTATION_SUMMARY.md` - Implementation details
- `QUICK_START.md` - This file

### 🔧 Source Files (Understand These)
- `main.c` - Original application
- `main_with_managers.c` - Example with managers
- `middleware/sensor_manager.c` - Sensor manager implementation
- `middleware/uart_manager.c` - UART manager implementation

---

## Next Steps

### For Development:
1. ✅ Start with Configuration A (Debug mode)
2. ✅ Verify sensor readings in terminal
3. ✅ Test error recovery (unplug sensor)
4. ✅ Adjust timing parameters if needed
5. ✅ Add your application logic

### For Production:
1. ✅ Switch to Configuration B (Production)
2. ✅ Test Modbus communication
3. ✅ Verify all sensors
4. ✅ Run 24-hour stability test
5. ✅ Deploy to field

### For Adding Sensors:
1. ✅ Read `CONFIGURATION_GUIDE.md` section "Adding New Sensors"
2. ✅ Create driver in `drivers/sensor_new/`
3. ✅ Add to `sensor_manager.h`
4. ✅ Implement in `sensor_manager.c`
5. ✅ Test and validate

---

## Cheat Sheet

### Sensor Manager API

```c
// Initialize all sensors
sensor_manager_init(&g_sensor_manager);

// Poll all sensors (call in main loop)
sensor_manager_poll(&g_sensor_manager);

// Check sensor status
sensor_status_e status = sensor_manager_get_status(&g_sensor_manager, 0);

// Force sensor reset
sensor_manager_reset_sensor(&g_sensor_manager, 0);

// Get statistics
uint32_t errors, zeros;
sensor_manager_get_stats(&g_sensor_manager, 0, &errors, &zeros);
```

### UART Manager API

```c
// Initialize UART
uart_manager_init(&g_uart_manager);

// Get current mode
comm_path_mode_e mode = uart_manager_get_usart1_mode(&g_uart_manager);

// Get statistics
uint32_t tx, rx, err;
uart_manager_get_stats(&g_uart_manager, 1, &tx, &rx, &err);
```

### Debug Output (when USART1_MODE = TTL)

```c
// String output
bsp_debug_write_str("Hello, World!\r\n");

// Integer output
bsp_debug_write_int("Counter: ", count, "\r\n");

// Fixed-point output (e.g., 245 / 10 = 24.5)
bsp_debug_write_fixed("Temp: ", 245, 10, 1, "C\r\n");

// Hex dump
bsp_debug_write_hex("Data: ", buffer, 8, "\r\n");
```

---

## Configuration Matrix

| Feature | Macro | Values | Default |
|---------|-------|--------|---------|
| **Enable SEN66** | `SENSOR_ENABLE_SEN66` | 0, 1 | 1 |
| **Enable CO** | `SENSOR_ENABLE_INFWIN_CO` | 0, 1 | 1 |
| **Poll Interval** | `SENSOR_POLL_INTERVAL_MS` | 100-10000 | 1000 |
| **Zero Filter** | `SENSOR_ENABLE_ZERO_FILTER` | 0, 1 | 1 |
| **USART1 Mode** | `USART1_MODE` | TTL, RS485, RS232, SDI12 | TTL |
| **TTL Baudrate** | `COMM_TTL_BAUDRATE` | Any | 115200 |
| **RS485 Baudrate** | `COMM_RS485_BAUDRATE` | Any | 9600 |

---

## Performance Metrics

| Metric | Value |
|--------|-------|
| **Flash Usage** | ~15KB (both sensors) |
| **RAM Usage** | ~2KB |
| **CPU Load** | <5% @ 64MHz |
| **Sensor Poll Rate** | 1 Hz (configurable) |
| **I2C Speed** | 100 kHz |
| **UART Speed** | 9600 or 115200 baud |
| **Watchdog Timeout** | 4 seconds |

---

## Support Resources

### 📚 Documentation
- **Full Guide**: `README.md`
- **Configuration**: `CONFIGURATION_GUIDE.md`
- **Architecture**: `ARCHITECTURE.md`
- **API Details**: Header files

### 🔍 Debugging
- Enable `USART1_MODE = COMM_PATH_MODE_TTL`
- Check initialization messages
- Monitor sensor readings
- Verify state transitions

### 🐛 Common Issues
- Sensor not detected → Check I2C wiring
- No debug output → Check UART mode
- Build errors → Run `make clean`
- Frequent errors → Increase error tolerance

---

## Quick Checklist

Before first use:
- [ ] Configured sensors in `sensor_manager.h`
- [ ] Selected UART mode in `uart_manager.h`
- [ ] Built firmware successfully
- [ ] Flashed to MCU
- [ ] Verified sensor initialization
- [ ] Tested communication

For production:
- [ ] Disabled debug mode
- [ ] Tested all sensors
- [ ] Verified Modbus communication
- [ ] Run 24-hour stability test
- [ ] Documented configuration

---

## Getting Help

1. **Check documentation**: Read `CONFIGURATION_GUIDE.md`
2. **Review examples**: See `main_with_managers.c`
3. **Enable debug**: Set `USART1_MODE = TTL`
4. **Check hardware**: Verify connections and power
5. **Test incrementally**: One sensor at a time

---

**You're ready to go! 🚀**

Start with debug mode, verify sensors work, then switch to production configuration.

---

**Last Updated**: July 15, 2026  
**Version**: 1.0  
**Motto**: "BE PART OF NATURE, PROTECT THE FUTURE"

# Serial Communication Troubleshooting

## Problem: Garbled Characters / Corrupt Output

Jika Anda melihat karakter aneh seperti:
```
« �r�����d�䒐l��䓀��d�|��d�|��n|�c���|sc
```

Ini adalah masalah **Baudrate Mismatch**.

---

## ✅ Solusi yang Sudah Diterapkan

Firmware telah diperbaiki dengan BRR (Baudrate Register) yang tepat:

```c
// Untuk System Clock 64 MHz
#define USART1_BRR_115200   (555U)   // 64,000,000 / 115,200 = 555.55
#define USART1_BRR_9600     (6667U)  // 64,000,000 / 9,600 = 6666.67
```

---

## 🔧 Konfigurasi Terminal Serial

### Settings yang Benar

| Parameter | Value |
|-----------|-------|
| **Baudrate** | **115200** bps |
| **Data bits** | 8 |
| **Parity** | None |
| **Stop bits** | 1 |
| **Flow control** | None |

### Konfigurasi Singkat: **115200 8N1**

---

## 📱 Software Terminal yang Direkomendasikan

### Windows

#### 1. PuTTY (Recommended)
```
Download: https://www.putty.org/
Configuration:
  - Connection Type: Serial
  - Serial line: COM3 (sesuaikan)
  - Speed: 115200
  - Data bits: 8
  - Stop bits: 1
  - Parity: None
  - Flow control: None
```

#### 2. TeraTerm
```
Download: https://ttssh2.osdn.jp/
Configuration:
  - Setup → Serial Port
  - Port: COM3
  - Baud rate: 115200
  - Data: 8 bit
  - Parity: none
  - Stop: 1 bit
  - Flow control: none
```

#### 3. Arduino Serial Monitor
```
- Select correct COM port
- Set baudrate to 115200
- Both NL & CR
```

### Linux/Mac

#### screen
```bash
screen /dev/ttyUSB0 115200
# Exit: Ctrl+A, then K
```

#### minicom
```bash
minicom -D /dev/ttyUSB0 -b 115200
# Exit: Ctrl+A, then X
```

#### cu
```bash
cu -l /dev/ttyUSB0 -s 115200
# Exit: ~.
```

---

## 🔍 Cara Cek COM Port

### Windows

#### Device Manager
```
1. Win + X → Device Manager
2. Expand "Ports (COM & LPT)"
3. Lihat "USB Serial Port (COMx)" atau "STMicroelectronics STLink Virtual COM Port"
4. Catat nomor COMx
```

#### PowerShell
```powershell
Get-WmiObject Win32_SerialPort | Select-Object Name,DeviceID
```

### Linux
```bash
# List all serial devices
ls /dev/tty*

# Devices with info
dmesg | grep tty

# USB serial devices
ls /dev/ttyUSB*
ls /dev/ttyACM*
```

### Mac
```bash
ls /dev/cu.*
```

---

## ⚠️ Common Issues

### Issue 1: No Output at All

**Checklist**:
- [ ] Board powered?
- [ ] UART TX pin connected to USB-UART RX?
- [ ] UART RX pin connected to USB-UART TX? (untuk 2-way comm)
- [ ] Ground connected?
- [ ] Correct COM port selected?
- [ ] Terminal open and configured?

**Pin Mapping (STM32G031)**:
```
USART1 (Debug TTL):
  TX: PA9 (pin 21) → Connect to USB-UART RX
  RX: PA10 (pin 22) → Connect to USB-UART TX
  GND: Connect to USB-UART GND
```

### Issue 2: Garbage Characters

**Cause**: Baudrate mismatch

**Solution**:
1. Verify terminal baudrate: **115200**
2. Reflash firmware dengan versi terbaru (sudah diperbaiki)
3. Power cycle board setelah flash

### Issue 3: Wrong Baudrate in Old Firmware

Jika menggunakan firmware lama dengan BRR 556:

```
Actual Baudrate = 64,000,000 / 556 = 115,107 bps
Error = (115,107 - 115,200) / 115,200 = -0.08%
```

Error kecil tapi bisa menyebabkan garbled text pada komunikasi panjang.

**Firmware baru menggunakan BRR 555**:
```
Actual Baudrate = 64,000,000 / 555 = 115,315 bps
Error = (115,315 - 115,200) / 115,200 = +0.1%
```

Lebih baik tapi masih ada error kecil.

**Solusi Optimal**: Gunakan BRR 555 (sudah diterapkan)

### Issue 4: Characters Missing or Duplicated

**Cause**: 
- Flow control enabled (harus None)
- Buffer overflow
- Interrupt priority issue

**Solution**:
1. Disable flow control di terminal
2. Pastikan firmware menggunakan interrupt-driven TX
3. Cek NVIC priority configuration

---

## 🧪 Test Output yang Diharapkan

Setelah flash firmware baru, Anda seharusnya melihat:

```
=================================
MERTANI BSP V1.0
UART MANAGER - TTL DEBUG MODE
=================================
[UART_MGR] USART1: TTL @ 115200 bps
[UART_MGR] USART2: RS485 @ 9600 bps
[SEN66] Initializing sensor...
[SEN66] Initialization successful
[SEN66] Starting measurement...
[SEN66] Measurement started, entering warmup phase
[SENSOR_MGR] SEN66 warming up...
[SENSOR_MGR] SEN66 warmup complete.
[SEN66] PM2.5:25.3 | Temp:24.5C | ...
```

---

## 🔧 Debugging Steps

### Step 1: Verify Hardware
```
1. Measure voltage on TX pin (should toggle 0-3.3V)
2. Use oscilloscope to check signal (optional)
3. Check continuity of connections
```

### Step 2: Verify Terminal Settings
```
1. Open terminal
2. Check COM port
3. Verify 115200 8N1
4. Disable all flow control
```

### Step 3: Test with Simple Output

Edit `main.c` untuk menambahkan test output sederhana:

```c
#ifdef USART1_DEBUG_MODE
    bsp_debug_write_str("\r\n=== SERIAL TEST ===\r\n");
    bsp_debug_write_str("0123456789\r\n");
    bsp_debug_write_str("ABCDEFGHIJKLMNOPQRSTUVWXYZ\r\n");
    bsp_debug_write_str("abcdefghijklmnopqrstuvwxyz\r\n");
    bsp_debug_write_str("===================\r\n\r\n");
#endif
```

Jika ini tampil dengan benar, maka UART bekerja normal.

### Step 4: Check Clock Configuration

Verify di `bsp_clock.c`:
```c
// HSI = 16 MHz
// PLL: 16MHz × 8 ÷ 2 = 64 MHz
#define BSP_PLLM    (0U)  // ÷1
#define BSP_PLLN    (8U)  // ×8
#define BSP_PLLR    (2U)  // ÷2
```

---

## 📊 Baudrate Calculation Reference

| System Clock | Baudrate | BRR Value | Actual Baud | Error % |
|--------------|----------|-----------|-------------|---------|
| 64 MHz | 9600 | 6667 | 9600.14 | +0.001% |
| 64 MHz | 19200 | 3333 | 19201.92 | +0.01% |
| 64 MHz | 38400 | 1667 | 38386.44 | -0.04% |
| 64 MHz | 57600 | 1111 | 57613.87 | +0.02% |
| 64 MHz | **115200** | **555** | **115315.32** | **+0.1%** |

**Formula**:
```
BRR = System_Clock / Baudrate
Actual_Baudrate = System_Clock / BRR
Error = (Actual - Target) / Target × 100%
```

---

## 🎯 Quick Fix Checklist

- [x] Firmware diperbaiki (BRR 556 → 555)
- [x] Build firmware baru
- [ ] Flash firmware ke board
- [ ] Set terminal ke 115200 8N1
- [ ] Disable flow control
- [ ] Connect UART pins (TX, RX, GND)
- [ ] Power cycle board
- [ ] Open terminal dan verify output

---

## 📞 Still Having Issues?

Jika masih ada masalah:

1. **Verify pin connections**:
   ```
   STM32 TX (PA9) → USB-UART RX
   STM32 RX (PA10) → USB-UART TX
   STM32 GND → USB-UART GND
   ```

2. **Test with loopback**:
   ```
   Connect TX to RX on USB-UART
   Type in terminal → should echo back
   ```

3. **Check USB-UART adapter**:
   ```
   Test with known working device
   Verify drivers installed
   ```

4. **Measure voltage levels**:
   ```
   3.3V TTL: 0V (low), 3.3V (high)
   5V TTL: 0V (low), 5V (high)
   ```
   Use level shifter if needed!

---

**Last Updated**: 15 Juli 2026  
**Firmware Version**: v1.0 (Fixed BRR)  
**Author**: DST0x

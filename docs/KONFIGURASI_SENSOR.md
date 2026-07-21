# Konfigurasi Sensor - CO dan SEN66

## Status Konfigurasi Saat Ini

### Sensor yang Aktif
✅ **SEN66** - Sensirion Air Quality Sensor (I2C)
✅ **Infwin CO Sensor** - Carbon Monoxide Sensor (UART/Modbus)
❌ **PMSX003** - Particulate Matter Sensor (DISABLED)

### Konfigurasi UART
- **USART1**: RS485 Mode (untuk CO Sensor via Modbus) - Baudrate 9600
- **USART2**: RS485 Mode (untuk Modbus Master) - Baudrate 9600

---

## Detail Konfigurasi

### File: `middleware/sensor_manager.h`

```c
#define SENSOR_ENABLE_SEN66         1   /* AKTIF - Sensirion SEN66 */
#define SENSOR_ENABLE_INFWIN_CO     1   /* AKTIF - Infwin CO Sensor */
#define SENSOR_ENABLE_PMSX003       0   /* NONAKTIF - PMSX003 */
```

### File: `middleware/uart_manager.h`

```c
#define USART1_MODE    COMM_PATH_MODE_RS485  /* RS485 untuk CO Sensor */
#define USART2_MODE    COMM_PATH_MODE_RS485  /* RS485 untuk Modbus Master */
```

---

## Spesifikasi Sensor

### 1. SEN66 Air Quality Sensor

**Interface**: I2C (0x6B)

**Parameter yang Diukur**:
- PM1.0, PM2.5, PM4.0, PM10 (Particulate Matter)
- Temperature (Suhu)
- Humidity (Kelembaban)
- VOC Index (Volatile Organic Compounds)
- NOx Index (Nitrogen Oxides)
- CO2 (Carbon Dioxide) - estimasi

**Waktu Warmup**: 30 detik (dikonfigurasi dari default 60 detik)

**Interval Polling**: 1000ms (1 detik)

### 2. Infwin CO Sensor

**Interface**: UART (USART1) via Modbus RTU

**Parameter yang Diukur**:
- CO (Carbon Monoxide) dalam ppm

**Komunikasi**:
- Protocol: Modbus RTU
- Mode: RS485
- Baudrate: 9600 bps
- Slave Address: Dikonfigurasi di sensor

**Waktu Warmup**: 30 detik

**Interval Polling**: Diatur oleh Modbus requests

---

## Koneksi Hardware

### SEN66 (I2C)
```
SEN66          STM32G031F8
-----          ------------
VDD    ----    3.3V
GND    ----    GND
SDA    ----    PB7 (I2C1_SDA)
SCL    ----    PB6 (I2C1_SCL)
```

### CO Sensor (USART1/RS485)
```
CO Sensor      STM32G031F8
---------      ------------
TX     ----    PA10 (USART1_RX)
RX     ----    PA9  (USART1_TX)
GND    ----    GND
VCC    ----    Power supply sensor
```

**Catatan**: Jika menggunakan RS485, diperlukan transceiver (MAX485 atau sejenisnya) dengan pin DE/RE

---

## Data Output Modbus

### Holding Registers (SEN66 + CO)

| Register | Parameter | Unit | Scaling | Range |
|----------|-----------|------|---------|-------|
| 0 | PM1.0 | µg/m³ | ×10 | 0-65535 |
| 1 | PM2.5 | µg/m³ | ×10 | 0-65535 |
| 2 | PM4.0 | µg/m³ | ×10 | 0-65535 |
| 3 | PM10 | µg/m³ | ×10 | 0-65535 |
| 4 | Temperature | °C | ×10 | -400 to 1250 |
| 5 | Humidity | %RH | ×10 | 0-1000 |
| 6 | VOC Index | - | ×10 | 0-5000 |
| 7 | NOx Index | - | ×10 | 0-5000 |
| 8 | CO₂ | ppm | ×1 | 0-40000 |
| 9 | CO | ppm | varies | Tergantung sensor |

**Contoh Pembacaan**:
- Register 0 = 125 → PM1.0 = 12.5 µg/m³
- Register 4 = 235 → Temperature = 23.5°C
- Register 9 = 15 → CO = 15 ppm (atau 1.5 ppm jika ×10)

---

## Fitur Sistem

### 1. Zero Filtering
```c
#define SENSOR_ENABLE_ZERO_FILTER   1
#define SENSOR_MAX_ZERO_COUNT       3
```

**Fungsi**: Jika sensor mengirim nilai 0 lebih dari 3 kali berturut-turut, sistem akan menggunakan nilai terakhir yang valid.

### 2. Error Handling
```c
#define SENSOR_MAX_ERRORS           5
#define SENSOR_RETRY_INTERVAL_MS    5000U
```

**Fungsi**: Jika sensor error lebih dari 5 kali, sistem akan mencoba reset sensor. Retry dilakukan setiap 5 detik.

### 3. Periodic Reset
```c
#define SENSOR_RESET_INTERVAL_MS    3600000U  /* 1 jam */
```

**Fungsi**: Sensor direset secara otomatis setiap 1 jam untuk mencegah hang dalam operasi jangka panjang.

### 4. LED Status Indicator
- **Blink lambat (500ms)**: Sistem normal, semua sensor OK
- **Blink cepat (100ms)**: Ada sensor yang error

---

## Build dan Flash

### Build Firmware
```powershell
# Clean dan build
.\build.ps1 -Clean -Build

# Build saja
.\build.ps1
```

### Flash ke Board
```powershell
# Build dan flash
.\build.ps1 -Flash
```

### Cek Memory Usage
```powershell
.\build.ps1 -Size
```

---

## Testing dan Debugging

### Mode Debug (USART1 TTL)

Untuk melihat output debug, ubah konfigurasi:

```c
// File: middleware/uart_manager.h
#define USART1_MODE    COMM_PATH_MODE_TTL  /* Debug mode */
```

Kemudian build ulang. Output akan tersedia di USART1 dengan baudrate 115200:

```
[MAIN] System initialized successfully.
[MAIN] Sensors configured:
  - SEN66 Air Quality Sensor
  - Infwin CO Sensor
[SEN66] PM1.0:12.5 | PM2.5:23.1 | Temperature:23.5C | ...
[STATUS] SEN66: RUNNING | Errors: 0 | Zeros: 0
```

### Mode Production (USART1 RS485)

Untuk operasi normal dengan CO sensor:

```c
// File: middleware/uart_manager.h
#define USART1_MODE    COMM_PATH_MODE_RS485  /* Production mode */
```

---

## Troubleshooting

### SEN66 Tidak Terdeteksi
1. Cek koneksi I2C (SDA/SCL)
2. Cek pull-up resistor pada I2C (4.7kΩ)
3. Cek power supply (3.3V)
4. Verifikasi address I2C (0x6B)

### CO Sensor Tidak Merespon
1. Cek koneksi UART (TX/RX)
2. Pastikan baudrate cocok (9600)
3. Cek transceiver RS485 (MAX485 dsb.) dan posisi demux komunikasi (PA0/PA1)
4. Verifikasi Modbus slave address
5. Cek power supply sensor

### Data Sensor Selalu 0
1. Sensor mungkin masih dalam periode warmup (30 detik)
2. Zero filtering mungkin aktif - tunggu beberapa pembacaan
3. Cek koneksi fisik sensor
4. Monitor LED indicator untuk status error

### Build Error
1. Pastikan ARM toolchain terinstall
2. Cek path di environment variables
3. Jalankan `.\build.ps1 -Clean` sebelum build ulang

---

## Modifikasi Konfigurasi

### Mengganti Interval Polling

Untuk mengubah kecepatan pembacaan sensor:

```c
// File: middleware/sensor_manager.h
#define SENSOR_POLL_INTERVAL_MS     500U   // Poll setiap 0.5 detik (lebih cepat)
// atau
#define SENSOR_POLL_INTERVAL_MS     5000U  // Poll setiap 5 detik (lebih hemat power)
```

### Menonaktifkan Zero Filtering

Jika ingin data mentah tanpa filtering:

```c
// File: middleware/sensor_manager.h
#define SENSOR_ENABLE_ZERO_FILTER   0   // Disable filtering
```

### Mengubah Waktu Warmup

Jika sensor Anda memerlukan waktu warmup berbeda:

```c
// File: middleware/sensor_manager.h
#define SENSOR_SEN66_WARMUP_MS      45000U  // 45 detik
#define SENSOR_CO_WARMUP_MS         60000U  // 60 detik
```

---

## Referensi File Penting

1. **Konfigurasi Sensor**: `middleware/sensor_manager.h`
2. **Konfigurasi UART**: `middleware/uart_manager.h`
3. **Main Program**: `main.c`
4. **Driver SEN66**: `drivers/sensor_sensirion_sen66/`
5. **Driver CO**: `drivers/sensor_infwin_co/`
6. **Modbus Handler**: `middleware/modbus/modbus_slave.c`

---

## Versi dan Update

**Versi Konfigurasi**: 1.0  
**Tanggal**: 18 Juli 2026  
**Target MCU**: STM32G031F8P6  
**Toolchain**: ARM GCC 15.2.1

---

## Kontak dan Support

Untuk pertanyaan atau issue terkait konfigurasi ini:
- Lihat dokumentasi lengkap di `CONFIGURATION_GUIDE.md`
- Cek troubleshooting di `SERIAL_TROUBLESHOOTING.md`
- Review arsitektur sistem di `ARCHITECTURE.md`

---

**Status Build Terakhir**: ✅ Success  
**Firmware Size**: (lihat output build untuk detail)


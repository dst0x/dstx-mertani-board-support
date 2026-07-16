# Driver Integration Guide - Sensor Manager

## Overview

Dokumen ini menjelaskan bagaimana driver sensor SEN66 dan Infwin CO terintegrasi dengan `sensor_manager` untuk menghasilkan sistem manajemen sensor yang terpusat dan modular.

**Tanggal Update**: 15 Juli 2026  
**Author**: DST0x

---

## Arsitektur Integrasi

```
Application (main.c)
        │
        ├─► sensor_manager_init()
        │   └─► Menghubungkan driver pointer
        │
        └─► sensor_manager_poll()
            │
            ├─► SEN66 Driver
            │   ├─► sensirion_sen66_init()
            │   ├─► sensirion_sen66_poll()
            │   └─► sensirion_sen66_reset()
            │
            └─► CO Sensor Driver
                ├─► sensor_co_init()
                ├─► sensor_co_poll()
                └─► sensor_co_reset()
```

---

## Fitur Integrasi

### 1. **Debug Output Terintegrasi**

Kedua driver sekarang mendukung debug output yang dapat diaktifkan/nonaktifkan:

```c
#ifdef USART1_DEBUG_MODE
    bsp_debug_write_str("[SEN66] Initializing sensor...\r\n");
#endif
```

**Output Debug yang Ditambahkan**:

**SEN66 Driver**:
- `[SEN66] Initializing sensor...`
- `[SEN66] Initialization successful`
- `[SEN66] I2C communication failed`
- `[SEN66] CRC verification failed`
- `[SEN66] Starting measurement...`
- `[SEN66] Measurement started, entering warmup phase`
- `[SEN66] Resetting sensor...`
- `[SEN66] Reset successful/failed`

**CO Sensor Driver**:
- `[CO_SENSOR] Initializing CO sensor...`
- `[CO_SENSOR] Initialization successful`
- `[CO_SENSOR] Warmup complete, entering running state`
- `[CO_SENSOR] Response timeout`
- `[CO_SENSOR] CRC error`
- `[CO_SENSOR] Invalid response address or function code`
- `[CO_SENSOR] Data received: XXX ppm`

### 2. **Fungsi Utility Tambahan**

Fungsi-fungsi inline telah ditambahkan untuk memudahkan integrasi dengan sensor_manager:

#### SEN66 Utility Functions

```c
/* Mengecek apakah sensor memiliki data valid */
bool sensirion_sen66_has_valid_data(const sen66_ctx_s *ctx);

/* Mengecek apakah sensor sedang warming up */
bool sensirion_sen66_is_warming_up(const sen66_ctx_s *ctx);

/* Mengecek apakah sensor sedang running */
bool sensirion_sen66_is_running(const sen66_ctx_s *ctx);

/* Mengecek apakah sensor mengalami error */
bool sensirion_sen66_has_error(const sen66_ctx_s *ctx);

/* Mendapatkan jumlah error */
uint8_t sensirion_sen66_get_error_count(const sen66_ctx_s *ctx);
```

#### CO Sensor Utility Functions

```c
/* Reset sensor (untuk recovery) */
status_e sensor_co_reset(co_ctx_s *ctx);

/* Mengecek apakah sensor memiliki data valid */
bool sensor_co_has_valid_data(const co_ctx_s *ctx);

/* Mendapatkan nilai CO terbaru */
uint16_t sensor_co_get_value(const co_ctx_s *ctx);
```

### 3. **State Management yang Konsisten**

Kedua driver menggunakan state machine yang konsisten:

**SEN66 States**:
```
UNINIT → IDLE → WARMING_UP → RUNNING
                     ↓
                   ERROR
```

**CO Sensor States**:
```
UNINIT → WARMING_UP → RUNNING
            ↓
          ERROR
```

### 4. **Error Handling Terintegrasi**

Driver melaporkan error ke sensor_manager yang kemudian menangani:
- **Automatic retry**: Sensor_manager mencoba reinisialisasi
- **Error counting**: Tracking jumlah error sebelum marking sensor sebagai failed
- **Recovery**: Otomatis mencoba recovery setelah interval tertentu

---

## Cara Menggunakan Driver dengan Sensor Manager

### Setup Dasar

```c
#include "middleware/sensor_manager.h"
#include "drivers/sensor_sensirion_sen66/sensirion_sen66.h"
#include "drivers/sensor_infwin_co/infwin_co_sensor.h"

/* Deklarasi contexts */
static sen66_ctx_s g_sen66_sensor = {0};
static sensor_manager_ctx_s g_sensor_manager = {0};

#ifdef USART1_MODBUS_MODE
    static co_ctx_s g_co_sensor = {0};
#endif

int main(void) {
    /* Inisialisasi hardware */
    bsp_systick_init();
    bsp_clock_init();
    bsp_gpio_init();
    bsp_i2c_init();
    
    /* Hubungkan driver ke sensor manager */
    g_sensor_manager.sen66_driver = &g_sen66_sensor;
    
    #ifdef USART1_MODBUS_MODE
        g_sensor_manager.co_driver = &g_co_sensor;
    #endif
    
    /* Inisialisasi sensor manager (akan memanggil init driver) */
    sensor_manager_init(&g_sensor_manager);
    
    /* Main loop */
    for (;;) {
        /* Polling semua sensor */
        sensor_manager_poll(&g_sensor_manager);
        
        /* Akses data sensor */
        if (sensirion_sen66_has_valid_data(&g_sen66_sensor)) {
            /* Gunakan data dari g_sen66_sensor.data */
        }
        
        #ifdef USART1_MODBUS_MODE
        if (sensor_co_has_valid_data(&g_co_sensor)) {
            uint16_t co_value = sensor_co_get_value(&g_co_sensor);
            /* Gunakan co_value */
        }
        #endif
        
        bsp_iwdg_refresh();
    }
}
```

---

## Fitur-Fitur yang Ditangani Sensor Manager

### 1. Zero Value Filtering

Sensor manager otomatis mendeteksi pembacaan nol dan menggunakan nilai valid terakhir:

```c
// Contoh dari sensor_manager.c
if (is_reading_zero(sen66->data.pm2_5)) {
    ctx->sen66_ctx.zero_count++;
    if (ctx->sen66_ctx.zero_count > SENSOR_MAX_ZERO_COUNT) {
        /* Gunakan last_valid_data */
        sen66->data = sen66->last_valid_data;
    }
} else {
    ctx->sen66_ctx.zero_count = 0;
    sen66->last_valid_data = sen66->data;
}
```

### 2. Periodic Reset

Sensor manager melakukan reset berkala untuk mencegah sensor hang:

```c
/* Di sensor_manager.h */
#define SENSOR_RESET_INTERVAL_MS    3600000U  // Reset setiap 1 jam

/* Di sensor_manager.c */
if (sensor_manager_needs_periodic_reset(last_reset_tick, now)) {
    sensor_manager_reset_sensor(&g_sensor_manager, 0);  // Reset SEN66
}
```

### 3. Error Recovery

Ketika sensor mengalami error, sensor_manager mencoba recovery:

```c
/* Sensor dalam error state */
if (ctx->sen66_ctx.status == SENSOR_STATUS_ERROR) {
    /* Tunggu interval retry */
    if (bsp_systick_elapsed(last_poll_tick, SENSOR_RETRY_INTERVAL_MS)) {
        /* Coba reinisialisasi */
        status = sensirion_sen66_init(sen66);
        if (status == STATUS_OK) {
            status = sensirion_sen66_start_measurement(sen66);
            /* Sensor kembali ke WARMING_UP state */
        }
    }
}
```

### 4. Warmup Management

Sensor manager menunggu sensor selesai warmup sebelum menandai sebagai RUNNING:

```c
if (ctx->sen66_ctx.status == SENSOR_STATUS_WARMING_UP) {
    if (is_warmup_complete(last_reset_tick, SENSOR_SEN66_WARMUP_MS)) {
        ctx->sen66_ctx.status = SENSOR_STATUS_RUNNING;
    }
}
```

---

## Konfigurasi Driver via Sensor Manager

### Mengaktifkan/Menonaktifkan Sensor

Edit `middleware/sensor_manager.h`:

```c
/* Aktifkan kedua sensor */
#define SENSOR_ENABLE_SEN66         1
#define SENSOR_ENABLE_INFWIN_CO     1

/* Atau nonaktifkan CO sensor */
#define SENSOR_ENABLE_SEN66         1
#define SENSOR_ENABLE_INFWIN_CO     0  // Driver tidak akan dikompilasi
```

### Mengatur Timing

```c
/* Polling interval (seberapa sering membaca sensor) */
#define SENSOR_POLL_INTERVAL_MS     1000U   // 1 detik

/* Retry interval (waktu tunggu setelah error) */
#define SENSOR_RETRY_INTERVAL_MS    5000U   // 5 detik

/* Reset interval (reset berkala) */
#define SENSOR_RESET_INTERVAL_MS    3600000U // 1 jam
```

### Mengatur Filtering

```c
/* Aktifkan zero filtering */
#define SENSOR_ENABLE_ZERO_FILTER   1

/* Jumlah maksimal zero berturut-turut sebelum menggunakan last valid */
#define SENSOR_MAX_ZERO_COUNT       3

/* Jumlah maksimal error sebelum sensor dianggap failed */
#define SENSOR_MAX_ERRORS           5
```

---

## API Reference

### SEN66 Driver API

```c
/* Inisialisasi sensor */
status_e sensirion_sen66_init(sen66_ctx_s *ctx);

/* Mulai pengukuran */
status_e sensirion_sen66_start_measurement(sen66_ctx_s *ctx);

/* Berhenti pengukuran */
status_e sensirion_sen66_stop_measurement(sen66_ctx_s *ctx);

/* Polling data sensor */
status_e sensirion_sen66_poll(sen66_ctx_s *ctx);

/* Reset sensor */
status_e sensirion_sen66_reset(sen66_ctx_s *ctx);

/* Cek apakah data ready */
bool sensirion_sen66_is_data_ready(sen66_ctx_s *ctx);

/* Utility functions */
bool sensirion_sen66_has_valid_data(const sen66_ctx_s *ctx);
bool sensirion_sen66_is_warming_up(const sen66_ctx_s *ctx);
bool sensirion_sen66_is_running(const sen66_ctx_s *ctx);
bool sensirion_sen66_has_error(const sen66_ctx_s *ctx);
uint8_t sensirion_sen66_get_error_count(const sen66_ctx_s *ctx);
```

### CO Sensor Driver API

```c
/* Inisialisasi sensor */
status_e sensor_co_init(co_ctx_s *ctx);

/* Kirim request ke sensor */
status_e sensor_co_send_request(co_ctx_s *ctx);

/* Polling data sensor */
status_e sensor_co_poll(co_ctx_s *ctx);

/* Reset sensor */
status_e sensor_co_reset(co_ctx_s *ctx);

/* Get state */
co_state_e sensor_co_get_state(const co_ctx_s *ctx);

/* Utility functions */
bool sensor_co_has_valid_data(const co_ctx_s *ctx);
uint16_t sensor_co_get_value(const co_ctx_s *ctx);

/* Receive byte (dipanggil dari interrupt) */
void sensor_co_rx_byte(co_ctx_s *ctx, uint8_t byte);
```

---

## Data Structure

### SEN66 Data Structure

```c
typedef struct {
    /* PM values (÷10 untuk mendapatkan µg/m³) */
    uint16_t pm1_0;
    uint16_t pm2_5;
    uint16_t pm4_0;
    uint16_t pm10;
    
    /* Number concentration */
    uint16_t nc0_5;
    uint16_t nc1_0;
    uint16_t nc2_5;
    uint16_t nc4_0;
    uint16_t nc10;
    uint16_t typ_size;
    
    /* Environmental data */
    int16_t  rh;        // Humidity (÷100 untuk mendapatkan %RH)
    int16_t  temp;      // Temperature (÷10 untuk mendapatkan °C)
    
    /* Gas data */
    int16_t  voc;       // VOC index (÷10)
    int16_t  nox;       // NOx index (÷10)
    uint16_t co2_ppm;   // CO2 dalam ppm
    
    /* Validity flags */
    bool     pm_valid;
    bool     env_valid;
    bool     gas_valid;
    bool     co2_valid;
} sen66_data_s;
```

### CO Sensor Data Structure

```c
typedef struct {
    uint32_t request_count;     // Jumlah request dikirim
    uint32_t response_count;    // Jumlah response diterima
    uint32_t timeout_count;     // Jumlah timeout
    uint32_t crc_error_count;   // Jumlah CRC error
    
    uint16_t co_value;          // Nilai CO dalam ppm
    bool valid;                 // Data valid flag
} co_data_s;
```

---

## Debug Mode

### Mengaktifkan Debug Output

Edit `middleware/uart_manager.h`:

```c
#define USART1_MODE    COMM_PATH_MODE_TTL  // Debug mode
```

### Output Debug yang Diharapkan

```
=================================
MERTANI BSP V1.0
UART MANAGER - TTL DEBUG MODE
=================================
[UART_MGR] USART1: TTL @ 115200 bps
[UART_MGR] USART2: RS485 @ 9600 bps
[SEN66] Initializing sensor...
[SEN66] Initialization successful
[SENSOR_MGR] SEN66 warming up...
[SEN66] Starting measurement...
[SEN66] Measurement started, entering warmup phase
[SENSOR_MGR] SEN66 warmup complete.
[SEN66] PM2.5:25.3 | Temp:24.5C | ...
```

---

## Troubleshooting

### Problem: Driver tidak dikompilasi

**Penyebab**: Sensor tidak diaktifkan di `sensor_manager.h`

**Solusi**:
```c
// Di sensor_manager.h
#define SENSOR_ENABLE_SEN66         1  // Pastikan 1
#define SENSOR_ENABLE_INFWIN_CO     1  // Pastikan 1
```

### Problem: Tidak ada debug output dari driver

**Penyebab**: `USART1_DEBUG_MODE` tidak didefinisikan

**Solusi**:
```c
// Di uart_manager.h
#define USART1_MODE    COMM_PATH_MODE_TTL  // Untuk debug
```

### Problem: Sensor stuck di WARMING_UP

**Penyebab**: Warmup time terlalu lama atau sensor tidak merespon

**Solusi**:
```c
// Di sensor_manager.h
#define SENSOR_SEN66_WARMUP_MS      60000U  // Coba turunkan jika perlu
#define SENSOR_CO_WARMUP_MS         30000U
```

### Problem: Frequent errors

**Penyebab**: Koneksi tidak stabil atau timing tidak tepat

**Solusi**:
```c
// Di sensor_manager.h
#define SENSOR_MAX_ERRORS           10  // Naikkan toleransi
#define SENSOR_RETRY_INTERVAL_MS    3000U  // Percepat retry
```

---

## Best Practices

### 1. Gunakan Sensor Manager untuk Kontrol

**❌ Jangan langsung:**
```c
// Jangan lakukan ini
sensirion_sen66_init(&g_sen66_sensor);
sensirion_sen66_start_measurement(&g_sen66_sensor);
for (;;) {
    sensirion_sen66_poll(&g_sen66_sensor);
}
```

**✅ Gunakan Sensor Manager:**
```c
// Lakukan ini
g_sensor_manager.sen66_driver = &g_sen66_sensor;
sensor_manager_init(&g_sensor_manager);
for (;;) {
    sensor_manager_poll(&g_sensor_manager);
}
```

### 2. Cek Status Sensor

```c
// Cek status sebelum mengakses data
sensor_status_e status = sensor_manager_get_status(&g_sensor_manager, 0);

if (status == SENSOR_STATUS_RUNNING) {
    // Sensor ready, data valid
    if (sensirion_sen66_has_valid_data(&g_sen66_sensor)) {
        // Gunakan data
    }
}
```

### 3. Handle Error Gracefully

```c
// Sensor manager menangani recovery otomatis
// Aplikasi hanya perlu cek apakah data valid
if (ctx->sen66_ctx.has_valid_data) {
    // Gunakan data
} else {
    // Gunakan default value atau last known good value
}
```

### 4. Monitor Statistics

```c
uint32_t errors, zeros;
sensor_manager_get_stats(&g_sensor_manager, 0, &errors, &zeros);

#ifdef USART1_DEBUG_MODE
    bsp_debug_write_int("Errors: ", errors, " | ");
    bsp_debug_write_int("Zeros: ", zeros, "\r\n");
#endif
```

---

## Performance Impact

| Feature | Flash | RAM | CPU |
|---------|-------|-----|-----|
| **SEN66 Driver** | ~4KB | ~100B | ~1% |
| **CO Driver** | ~2KB | ~100B | ~0.5% |
| **Debug Output** | +1KB | +256B | +0.5% |
| **Utility Functions** | +200B | 0B | <0.1% |

**Total Overhead**: Sangat minimal, cocok untuk STM32G0.

---

## Summary

✅ **Driver terintegrasi penuh dengan sensor_manager**  
✅ **Debug output informatif untuk troubleshooting**  
✅ **Fungsi utility untuk kemudahan akses**  
✅ **Error handling dan recovery otomatis**  
✅ **Zero filtering dan periodic reset**  
✅ **Modular dan mudah dikonfigurasi**

Kedua driver sekarang bekerja seamlessly dengan sensor_manager, memberikan sistem manajemen sensor yang robust dan mudah digunakan.

---

**Author**: DST0x  
**Date**: 15 Juli 2026  
**Version**: 1.0

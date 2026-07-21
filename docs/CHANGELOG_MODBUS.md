# Changelog - Modbus Protocol Update

## Tanggal: 18 Juli 2026

### Perubahan yang Dilakukan

#### 1. Menghilangkan Custom Header (3 Byte Prefix)

**Sebelumnya:**
```
Response Frame:
[0x00] [0x00] [0x48] [Slave ID] [Function] [Byte Count] [Data...] [CRC Low] [CRC High]
 └─── Custom Header ────┘ └──────────── Standard Modbus Response ───────────────────┘
```

**Sekarang (Standard Modbus):**
```
Response Frame:
[Slave ID] [Function] [Byte Count] [Data...] [CRC Low] [CRC High]
└──────────────────── Standard Modbus Response ───────────────────┘
```

**Alasan:**
- Custom header `0x00 0x00 0x48` tidak sesuai dengan standar Modbus RTU
- Menyebabkan incompatibility dengan Modbus master devices
- CRC calculation menjadi non-standard
- Device Modbus master tidak dapat mem-parsing response dengan benar

#### 2. Standard Modbus CRC Calculation

**Sebelumnya:**
```c
/* CRC calculated ONLY on Modbus portion (after 0x30 header) */
*tx_len = append_crc(&ctx->tx_buf[3U], (uint16_t)(3U + byte_cnt)) + 3U; /* +3 for header */
```

**Sekarang:**
```c
/* Standard Modbus CRC calculation */
*tx_len = append_crc(ctx->tx_buf, (uint16_t)(3U + byte_cnt));
```

**Alasan:**
- CRC harus dihitung dari awal frame (Slave ID)
- Sesuai dengan Modbus RTU specification
- Compatible dengan semua Modbus master devices

---

## Dampak Perubahan

### ✅ Keuntungan:
1. **Standard Compliance**: Response sepenuhnya sesuai Modbus RTU standard
2. **Universal Compatibility**: Kompatibel dengan semua Modbus master devices
3. **Simplified Protocol**: Tidak ada custom parsing yang diperlukan
4. **Proper CRC**: CRC calculation mengikuti standard

### ⚠️ Yang Perlu Diperhatikan:

**Jika Anda sudah memiliki software/device Modbus master yang menggunakan firmware lama:**
- Software tersebut harus diupdate untuk tidak mengharapkan 3-byte header
- Parsing response harus diubah ke standard Modbus format
- CRC validation harus menggunakan standard Modbus method

---

## Contoh Response Frame

### Request:
```
Read Holding Registers (Function 0x03)
Slave ID:        0x01
Function:        0x03
Start Address:   0x0000
Quantity:        9 registers (0x00 - 0x08)

Frame: 01 03 00 00 00 09 [CRC]
```

### Response (Format Baru - Standard):
```
Slave ID:        0x01
Function:        0x03
Byte Count:      0x12 (18 bytes = 9 registers × 2)
Data[0-1]:       PM1.0  (Register 0x00)
Data[2-3]:       PM2.5  (Register 0x01)
Data[4-5]:       PM4.0  (Register 0x02)
Data[6-7]:       PM10   (Register 0x03)
Data[8-9]:       RH     (Register 0x04)
Data[10-11]:     Temp   (Register 0x05)
Data[12-13]:     VOC    (Register 0x06)
Data[14-15]:     NOx    (Register 0x07)
Data[16-17]:     CO2    (Register 0x08)
CRC[0-1]:        CRC16 (calculated from byte 0 to byte 20)

Frame: 01 03 12 [18 data bytes] [CRC Low] [CRC High]
Total: 23 bytes
```

### Response (Format Lama - Non-Standard):
```
Custom Header:   0x00 0x00 0x48
Slave ID:        0x01
Function:        0x03
Byte Count:      0x12
Data:            [18 data bytes]
CRC:             [CRC of bytes after header]

Frame: 00 00 48 01 03 12 [18 data bytes] [CRC]
Total: 26 bytes
```

---

## Testing

### Test dengan Modbus Poll / QModMaster:

**Settings:**
```
Communication:
- Port: COM port yang sesuai
- Baud Rate: 9600
- Parity: None
- Data Bits: 8
- Stop Bits: 1

Modbus:
- Mode: RTU
- Slave ID: 1 (0x01)
```

**Read Test:**
```
Function: Read Holding Registers (0x03)
Address: 0
Length: 9

Expected Response:
- 9 registers dengan data sensor
- PM1.0, PM2.5, PM4.0, PM10, RH, Temp, VOC, NOx, CO2
```

### Python Test Script:

```python
import minimalmodbus
import serial

# Setup
instrument = minimalmodbus.Instrument('COM3', 1)  # COM3, slave ID 1
instrument.serial.baudrate = 9600
instrument.serial.parity = serial.PARITY_NONE
instrument.serial.stopbits = 1
instrument.serial.timeout = 1

# Read sensors
try:
    pm1_0 = instrument.read_register(0, 1)  # Register 0, 1 decimal
    pm2_5 = instrument.read_register(1, 1)
    pm4_0 = instrument.read_register(2, 1)
    pm10  = instrument.read_register(3, 1)
    rh    = instrument.read_register(4, 1)
    temp  = instrument.read_register(5, 1)
    voc   = instrument.read_register(6, 0)
    nox   = instrument.read_register(7, 0)
    co2   = instrument.read_register(8, 0)
    
    print(f"PM1.0: {pm1_0} µg/m³")
    print(f"PM2.5: {pm2_5} µg/m³")
    print(f"PM4.0: {pm4_0} µg/m³")
    print(f"PM10:  {pm10} µg/m³")
    print(f"RH:    {rh} %")
    print(f"Temp:  {temp} °C")
    print(f"VOC:   {voc}")
    print(f"NOx:   {nox}")
    print(f"CO2:   {co2} ppm")
    
except Exception as e:
    print(f"Error: {e}")
```

---

## Migration Guide

### Untuk Pengguna Firmware Lama:

**Step 1: Update Firmware**
```powershell
.\build.ps1 -Flash
```

**Step 2: Update Modbus Master Software**

Jika menggunakan custom software yang mem-parse response:

**HAPUS:**
```python
# Old code - remove this
header = response[0:3]  # 0x00 0x00 0x48
if header != b'\x00\x00\x48':
    raise ValueError("Invalid header")
modbus_data = response[3:]  # Skip header
```

**GUNAKAN:**
```python
# New code - standard Modbus
# No header stripping needed
modbus_data = response  # Standard Modbus frame
```

**Step 3: Verify CRC**

Pastikan CRC validation menggunakan standard method:

```python
import struct

def calc_crc(data):
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x0001:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc

# Validate
data_without_crc = response[:-2]
crc_received = struct.unpack('<H', response[-2:])[0]  # Little-endian
crc_calculated = calc_crc(data_without_crc)

if crc_received != crc_calculated:
    raise ValueError("CRC mismatch")
```

---

## Files Modified

1. **middleware/modbus/modbus_slave.c**
   - Removed custom header insertion
   - Changed CRC calculation to standard method
   - Updated response frame building

---

## Verification Checklist

Setelah update firmware, pastikan:

- [ ] Build firmware berhasil tanpa error
- [ ] Flash firmware ke board
- [ ] Test komunikasi Modbus dengan master device
- [ ] Verifikasi data sensor dapat dibaca dengan benar
- [ ] CRC validation passed
- [ ] Tidak ada error pada Modbus master
- [ ] Response time normal (< 100ms)

---

## Troubleshooting

### Problem: "CRC Error" pada Modbus Master

**Solution:**
- Pastikan firmware baru sudah di-flash
- Reset board setelah flash
- Verify baudrate dan parity settings sama di master dan slave

### Problem: "No Response" dari Device

**Solution:**
- Cek koneksi RS485 (A, B, GND)
- Verify slave ID (default: 0x01)
- Cek termination resistor (120Ω)
- Test dengan tool Modbus Poll untuk isolasi masalah

### Problem: Data Sensor Tidak Valid

**Solution:**
- Tunggu warmup sensor (30-60 detik)
- Cek register 100 (0x64) untuk status SEN66
- Cek register 103 (0x67) untuk status CO sensor
- Baca register debug (0x64 - 0x6D) untuk error count

---

## Referensi

- **Modbus RTU Specification**: MODBUS over Serial Line Specification V1.02
- **Standard Function Codes**: 
  - 0x03: Read Holding Registers
  - 0x04: Read Input Registers
  - 0x06: Write Single Register

---

**Document Version:** 1.0  
**Author:** DST0x  
**Date:** 18 Juli 2026


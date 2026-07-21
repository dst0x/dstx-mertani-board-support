# Build Guide - Mertani Board Support Package

## Prerequisites

### 1. ARM GCC Toolchain ✅ (Sudah Terinstall)

Anda sudah memiliki ARM toolchain di:
```
C:\Program Files (x86)\Arm\GNU Toolchain mingw-w64-i686-arm-none-eabi\
```

### 2. Make Utility ❌ (Perlu Diinstall)

Ada beberapa opsi untuk menginstall `make`:

#### Option A: Install Make via Chocolatey (Recommended)

1. Install Chocolatey (jika belum):
   ```powershell
   # Run PowerShell as Administrator
   Set-ExecutionPolicy Bypass -Scope Process -Force
   [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072
   iex ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))
   ```

2. Install make:
   ```powershell
   choco install make
   ```

#### Option B: Install MSYS2 (Provides make and more)

1. Download MSYS2: https://www.msys2.org/
2. Install MSYS2
3. Open MSYS2 terminal
4. Install make:
   ```bash
   pacman -S make
   ```
5. Add MSYS2 to PATH:
   ```
   C:\msys64\usr\bin
   ```

#### Option C: Manual Download

1. Download make for Windows: http://gnuwin32.sourceforge.net/packages/make.htm
2. Install ke folder yang sudah ada di PATH
3. Atau tambahkan folder install ke PATH

#### Option D: Use Build Script (No Make Required!)

Gunakan PowerShell build script yang sudah saya buat (lihat di bawah).

---

## Build Options

### Option 1: Using Makefile (Setelah Install Make)

```bash
# Clean previous build
make clean

# Build
make

# Flash to board
make flash

# View memory usage
make size

# Generate disassembly
make dump
```

### Option 2: Using PowerShell Build Script ⭐ (Recommended for Windows)

Saya telah membuat `build.ps1` yang tidak memerlukan make!

```powershell
# Build
.\build.ps1

# Build and flash
.\build.ps1 -Flash

# Clean build
.\build.ps1 -Clean

# Clean and build
.\build.ps1 -Clean -Build

# Show help
.\build.ps1 -Help
```

---

## Verifikasi Toolchain

Cek apakah ARM toolchain sudah terinstall dengan benar:

```powershell
# Cek GCC
arm-none-eabi-gcc --version

# Cek linker
arm-none-eabi-ld --version

# Cek objcopy
arm-none-eabi-objcopy --version

# Cek size
arm-none-eabi-size --version
```

Expected output:
```
arm-none-eabi-gcc (Arm GNU Toolchain ...) X.X.X
Copyright (C) 2023 Free Software Foundation, Inc.
```

---

## Flash Tools

### Option 1: OpenOCD + ST-Link

Install OpenOCD:
```powershell
choco install openocd
```

Flash command:
```bash
openocd -f interface/stlink.cfg -f target/stm32g0x.cfg \
  -c "program build/mertani_board_support.bin verify reset exit 0x08000000"
```

### Option 2: STM32CubeProgrammer (GUI)

1. Download: https://www.st.com/en/development-tools/stm32cubeprog.html
2. Install
3. Connect board
4. Load `build/mertani_board_support.hex`
5. Click "Download"

### Option 3: ST-Link Utility

1. Download: https://www.st.com/en/development-tools/stsw-link004.html
2. Install
3. Connect → Open File → Program & Verify

---

## Troubleshooting

### Problem: "make: command not found"

**Solution**: Install make (see Prerequisites) atau gunakan `build.ps1`

### Problem: "arm-none-eabi-gcc: command not found"

**Solution**: 
1. Cek PATH environment variable
2. Tambahkan ke PATH:
   ```
   C:\Program Files (x86)\Arm\GNU Toolchain mingw-w64-i686-arm-none-eabi\bin
   ```

### Problem: Build errors

**Solution**:
```bash
# Clean dan rebuild
make clean
make

# Atau dengan PowerShell script
.\build.ps1 -Clean -Build
```

### Problem: Flash errors

**Checklist**:
- [ ] ST-Link connected?
- [ ] Board powered?
- [ ] Correct flash tool installed?
- [ ] Drivers installed for ST-Link?

---

## Quick Start (Windows)

**Jika make belum terinstall**, gunakan PowerShell script:

```powershell
# 1. Open PowerShell di folder project
cd "C:\Users\techm\Dani\Codes\DST0x\mertani_board_support_v0.1"

# 2. Build project
.\build.ps1

# 3. Flash ke board (memerlukan OpenOCD atau STM32CubeProgrammer)
.\build.ps1 -Flash
```

---

## Build Output

Setelah build sukses, Anda akan mendapat:

```
build/
├── mertani_board_support.elf    # Executable with debug symbols
├── mertani_board_support.hex    # Intel HEX format
├── mertani_board_support.bin    # Binary format
└── mertani_board_support.map    # Memory map
```

**File sizes** (approximate):
- ELF: ~60KB
- HEX: ~30KB  
- BIN: ~15KB

---

## Environment Setup (One-time)

### Add ARM GCC to PATH

1. Open System Properties
2. Environment Variables
3. Edit PATH
4. Add:
   ```
   C:\Program Files (x86)\Arm\GNU Toolchain mingw-w64-i686-arm-none-eabi\bin
   ```

### Add OpenOCD to PATH (if installed)

Add to PATH:
```
C:\Program Files\OpenOCD\bin
```

---

## Recommended Development Setup

1. **IDE**: VS Code with extensions:
   - C/C++
   - Cortex-Debug
   - ARM Assembly

2. **Build Tool**: 
   - PowerShell script (untuk Windows, no dependencies)
   - Atau install make via Chocolatey

3. **Flash Tool**: 
   - STM32CubeProgrammer (GUI, easy)
   - Atau OpenOCD (command line, automation)

4. **Debug Tool**: 
   - OpenOCD + GDB
   - Atau STM32CubeIDE (integrated)

---

## Command Reference

### Make Commands (if installed)

| Command | Description |
|---------|-------------|
| `make` | Build project |
| `make clean` | Clean build artifacts |
| `make flash` | Flash to board |
| `make size` | Show memory usage |
| `make dump` | Generate disassembly |

### PowerShell Script Commands

| Command | Description |
|---------|-------------|
| `.\build.ps1` | Build project |
| `.\build.ps1 -Clean` | Clean only |
| `.\build.ps1 -Clean -Build` | Clean and build |
| `.\build.ps1 -Flash` | Build and flash |
| `.\build.ps1 -Help` | Show help |

---

## Next Steps

1. ✅ Verify ARM toolchain: `arm-none-eabi-gcc --version`
2. ⬜ Install make OR use PowerShell script
3. ⬜ Build project: `.\build.ps1`
4. ⬜ Flash to board
5. ⬜ Connect UART for debug output (115200 baud)

---

**Last Updated**: 15 Juli 2026  
**Author**: DST0x

#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Build script untuk Mertani Board Support Package
    
.DESCRIPTION
    Script PowerShell untuk build firmware tanpa memerlukan make.
    Compatible dengan Windows PowerShell dan PowerShell Core.
    
.PARAMETER Clean
    Clean build artifacts
    
.PARAMETER Build
    Build project (default jika tidak ada parameter)
    
.PARAMETER Flash
    Flash firmware ke board setelah build
    
.PARAMETER Size
    Show memory usage
    
.PARAMETER Help
    Show help message
    
.EXAMPLE
    .\build.ps1
    Build project
    
.EXAMPLE
    .\build.ps1 -Clean
    Clean build artifacts
    
.EXAMPLE
    .\build.ps1 -Clean -Build
    Clean dan build
    
.EXAMPLE
    .\build.ps1 -Flash
    Build dan flash ke board
    
.NOTES
    Author: DST0x
    Date: 15 Juli 2026
    Version: 1.0
#>

param(
    [switch]$Clean,
    [switch]$Build,
    [switch]$Flash,
    [switch]$Size,
    [switch]$Help
)

# Configuration
$TARGET = "mertani_board_support"
$BUILD_DIR = "build"
$LD_SCRIPT = "STM32G031F8PX_FLASH.ld"

# Toolchain
$CC = "arm-none-eabi-gcc"
$OBJCOPY = "arm-none-eabi-objcopy"
$OBJDUMP = "arm-none-eabi-objdump"
$SIZEUTIL = "arm-none-eabi-size"

# MCU flags
$MCU_FLAGS = @(
    "-mcpu=cortex-m0plus",
    "-mthumb",
    "-mfloat-abi=soft"
)

# Include paths
$INCLUDES = @(
    "-I.",
    "-ICMSIS/Device/ST/STM32G0xx/Include",
    "-ICMSIS/Include"
)

# Defines
$DEFINES = @(
    "-DSTM32G031xx"
)

# Compiler flags
$CFLAGS = $MCU_FLAGS + $INCLUDES + $DEFINES + @(
    "-std=c11",
    "-Wall",
    "-Wextra",
    "-Wno-unused-parameter",
    "-ffunction-sections",
    "-fdata-sections",
    "-ffreestanding",
    "-fno-common",
    "-Os"
)

# Linker flags
$LDFLAGS = $MCU_FLAGS + @(
    "-T$LD_SCRIPT",
    "-Wl,--gc-sections",
    "-Wl,-Map=$BUILD_DIR/$TARGET.map",
    "-Wl,--print-memory-usage",
    "-nostartfiles",
    "-nostdlib",
    "-lc",
    "-lgcc"
)

# Source files
$SOURCES = @(
    "main.c",
    "bsp/bsp_clock.c",
    "bsp/bsp_flash.c",
    "bsp/bsp_gpio.c",
    "bsp/bsp_i2c.c",
    "bsp/bsp_iwdg.c",
    "bsp/bsp_systick.c",
    "bsp/bsp_uart.c",
    "platform/stm32g0/startup_stm32g031xx.c",
    "drivers/sensor_sensirion_sen66/sensirion_sen66.c",
    "drivers/sensor_infwin_co/infwin_co_sensor.c",
    "drivers/sensor_pmsx003/pmsx003_sensor.c",
    "middleware/modbus/modbus_crc.c",
    "middleware/modbus/modbus_slave.c",
    "middleware/sensor_manager.c",
    "middleware/uart_manager.c"
)

# Colors for output
$COLOR_GREEN = "Green"
$COLOR_YELLOW = "Yellow"
$COLOR_RED = "Red"
$COLOR_CYAN = "Cyan"

# Functions
function Show-Help {
    Write-Host ""
    Write-Host "Mertani Board Support Package - Build Script" -ForegroundColor $COLOR_CYAN
    Write-Host "============================================" -ForegroundColor $COLOR_CYAN
    Write-Host ""
    Write-Host "Usage:" -ForegroundColor $COLOR_GREEN
    Write-Host "  .\build.ps1 [options]"
    Write-Host ""
    Write-Host "Options:" -ForegroundColor $COLOR_GREEN
    Write-Host "  -Clean       Clean build artifacts"
    Write-Host "  -Build       Build project (default)"
    Write-Host "  -Flash       Flash firmware to board"
    Write-Host "  -Size        Show memory usage"
    Write-Host "  -Help        Show this help message"
    Write-Host ""
    Write-Host "Examples:" -ForegroundColor $COLOR_GREEN
    Write-Host "  .\build.ps1                  Build project"
    Write-Host "  .\build.ps1 -Clean           Clean only"
    Write-Host "  .\build.ps1 -Clean -Build    Clean and build"
    Write-Host "  .\build.ps1 -Flash           Build and flash"
    Write-Host "  .\build.ps1 -Size            Show memory usage"
    Write-Host ""
}

function Test-Toolchain {
    Write-Host "Checking toolchain..." -ForegroundColor $COLOR_CYAN
    
    try {
        $version = & $CC --version 2>&1 | Select-Object -First 1
        Write-Host "  OK $version" -ForegroundColor $COLOR_GREEN
        return $true
    } catch {
        Write-Host "  X ARM toolchain not found!" -ForegroundColor $COLOR_RED
        Write-Host "  Please install arm-none-eabi-gcc and add to PATH" -ForegroundColor $COLOR_YELLOW
        return $false
    }
}

function Clean-Build {
    Write-Host "Cleaning build artifacts..." -ForegroundColor $COLOR_CYAN
    
    if (Test-Path $BUILD_DIR) {
        Remove-Item -Recurse -Force $BUILD_DIR
        Write-Host "  OK Cleaned." -ForegroundColor $COLOR_GREEN
    } else {
        Write-Host "  OK Already clean." -ForegroundColor $COLOR_GREEN
    }
}

function Build-Project {
    Write-Host "Building project..." -ForegroundColor $COLOR_CYAN
    
    # Create build directory
    if (-not (Test-Path $BUILD_DIR)) {
        New-Item -ItemType Directory -Path $BUILD_DIR | Out-Null
    }
    
    # Compile each source file
    $objects = @()
    $success = $true
    
    foreach ($source in $SOURCES) {
        $obj = "$BUILD_DIR/$($source -replace '\.c$','.o' -replace '/','_' -replace '\\','_')"
        $objects += $obj
        
        Write-Host "  CC   $source" -ForegroundColor $COLOR_YELLOW
        
        $output = & $CC $CFLAGS -c $source -o $obj 2>&1
        
        if ($LASTEXITCODE -ne 0) {
            Write-Host "  X Compilation failed for $source" -ForegroundColor $COLOR_RED
            Write-Host $output -ForegroundColor $COLOR_RED
            $success = $false
            break
        }
    }
    
    if (-not $success) {
        return $false
    }
    
    # Link
    Write-Host "  LD   $BUILD_DIR/$TARGET.elf" -ForegroundColor $COLOR_YELLOW
    
    $output = & $CC $LDFLAGS -o "$BUILD_DIR/$TARGET.elf" $objects 2>&1
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "  X Linking failed" -ForegroundColor $COLOR_RED
        Write-Host $output -ForegroundColor $COLOR_RED
        return $false
    }
    
    # Generate HEX
    Write-Host "  HEX  $BUILD_DIR/$TARGET.hex" -ForegroundColor $COLOR_YELLOW
    & $OBJCOPY -O ihex "$BUILD_DIR/$TARGET.elf" "$BUILD_DIR/$TARGET.hex" | Out-Null
    
    # Generate BIN
    Write-Host "  BIN  $BUILD_DIR/$TARGET.bin" -ForegroundColor $COLOR_YELLOW
    & $OBJCOPY -O binary "$BUILD_DIR/$TARGET.elf" "$BUILD_DIR/$TARGET.bin" | Out-Null
    
    # Show size
    Write-Host ""
    Write-Host "Memory Usage:" -ForegroundColor $COLOR_CYAN
    & $SIZEUTIL "$BUILD_DIR/$TARGET.elf"
    
    Write-Host ""
    Write-Host "  OK Build successful!" -ForegroundColor $COLOR_GREEN
    Write-Host ""
    Write-Host "Output files:" -ForegroundColor $COLOR_CYAN
    Write-Host "  - $BUILD_DIR/$TARGET.elf" -ForegroundColor $COLOR_GREEN
    Write-Host "  - $BUILD_DIR/$TARGET.hex" -ForegroundColor $COLOR_GREEN
    Write-Host "  - $BUILD_DIR/$TARGET.bin" -ForegroundColor $COLOR_GREEN
    
    return $true
}

function Flash-Board {
    Write-Host "Flashing to board..." -ForegroundColor $COLOR_CYAN
    
    # Check if STM32CubeProgrammer CLI is available
    try {
        $stm32cli = Get-Command STM32_Programmer_CLI -ErrorAction Stop
        
        Write-Host "  Using STM32CubeProgrammer CLI..." -ForegroundColor $COLOR_YELLOW
        
        $flashCmd = @(
            "-c", "port=SWD",
            "-w", "$BUILD_DIR/$TARGET.hex",
            "-v",
            "-rst"
        )
        
        & STM32_Programmer_CLI $flashCmd
        
        if ($LASTEXITCODE -eq 0) {
            Write-Host ""
            Write-Host "  OK Flash successful!" -ForegroundColor $COLOR_GREEN
            return
        } else {
            Write-Host ""
            Write-Host "  X Flash failed!" -ForegroundColor $COLOR_RED
            Write-Host "  Check if board is connected and ST-Link drivers are installed" -ForegroundColor $COLOR_YELLOW
            return
        }
        
    } catch {
        # STM32CubeProgrammer not found, try OpenOCD
    }
    
    # Check if OpenOCD is available
    try {
        $null = Get-Command openocd -ErrorAction Stop
        
        Write-Host "  Using OpenOCD..." -ForegroundColor $COLOR_YELLOW
        
        $flashCmd = @(
            "-f", "interface/stlink.cfg",
            "-f", "target/stm32g0x.cfg",
            "-c", "program $BUILD_DIR/$TARGET.bin verify reset exit 0x08000000"
        )
        
        & openocd $flashCmd
        
        if ($LASTEXITCODE -eq 0) {
            Write-Host "  OK Flash successful!" -ForegroundColor $COLOR_GREEN
        } else {
            Write-Host "  X Flash failed!" -ForegroundColor $COLOR_RED
        }
        
    } catch {
        Write-Host "  X No flash tool found!" -ForegroundColor $COLOR_RED
        Write-Host ""
        Write-Host "Please use one of these methods:" -ForegroundColor $COLOR_YELLOW
        Write-Host ""
        Write-Host "Method 1 - STM32CubeProgrammer (GUI):" -ForegroundColor $COLOR_CYAN
        Write-Host "  1. Open STM32CubeProgrammer" -ForegroundColor $COLOR_YELLOW
        Write-Host "  2. Connect to board (ST-Link)" -ForegroundColor $COLOR_YELLOW
        Write-Host "  3. Load file: $BUILD_DIR/$TARGET.hex" -ForegroundColor $COLOR_YELLOW
        Write-Host "  4. Click 'Download'" -ForegroundColor $COLOR_YELLOW
        Write-Host ""
        Write-Host "Method 2 - Install OpenOCD:" -ForegroundColor $COLOR_CYAN
        Write-Host "  choco install openocd" -ForegroundColor $COLOR_YELLOW
    }
}

function Show-Size {
    if (Test-Path "$BUILD_DIR/$TARGET.elf") {
        Write-Host "Memory Usage:" -ForegroundColor $COLOR_CYAN
        & $SIZEUTIL "$BUILD_DIR/$TARGET.elf"
    } else {
        Write-Host "  X Build first!" -ForegroundColor $COLOR_RED
    }
}

# Main script
Write-Host ""
Write-Host "=================================" -ForegroundColor $COLOR_CYAN
Write-Host "Mertani Board Support Package" -ForegroundColor $COLOR_CYAN
Write-Host "Build Script v1.0" -ForegroundColor $COLOR_CYAN
Write-Host "=================================" -ForegroundColor $COLOR_CYAN
Write-Host ""

# Show help if requested
if ($Help) {
    Show-Help
    exit 0
}

# Check toolchain
if (-not (Test-Toolchain)) {
    exit 1
}

Write-Host ""

# Execute requested actions
$actionTaken = $false

if ($Clean) {
    Clean-Build
    $actionTaken = $true
}

if ($Build -or (-not $actionTaken -and -not $Flash -and -not $Size)) {
    if (-not (Build-Project)) {
        exit 1
    }
    $actionTaken = $true
}

if ($Flash) {
    if (-not $actionTaken) {
        if (-not (Build-Project)) {
            exit 1
        }
    }
    Flash-Board
    $actionTaken = $true
}

if ($Size) {
    Show-Size
    $actionTaken = $true
}

Write-Host ""
Write-Host "Done!" -ForegroundColor $COLOR_GREEN
Write-Host ""

exit 0

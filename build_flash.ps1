<#
.SYNOPSIS
    Build and optionally flash SEN66-G031 firmware.

.DESCRIPTION
    Compiles STM32G031F8P6 firmware using arm-none-eabi-gcc (no HAL).
    Optionally flashes via OpenOCD + ST-Link.

.PARAMETER Flash
    If specified, flash the firmware after a successful build.

.PARAMETER Clean
    If specified, delete the build directory before compiling.

.EXAMPLE
    .\build_flash.ps1
    .\build_flash.ps1 -Flash
    .\build_flash.ps1 -Clean -Flash
#>
param(
    [switch]$Flash,
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------
$TARGET    = "mertani_board_support"
$BUILD     = "build"
$LD_SCRIPT = "STM32G031F8PX_FLASH.ld"

# CMSIS path — using local CMSIS directory
$CMSIS_ROOT = "CMSIS"
if (-not (Test-Path $CMSIS_ROOT)) {
    Write-Host "[ERROR] CMSIS not found at $CMSIS_ROOT" -ForegroundColor Red
    Write-Host "        Please run the setup to download CMSIS headers" -ForegroundColor Red
    exit 1
}

$CC      = "arm-none-eabi-gcc"
$OBJCOPY = "arm-none-eabi-objcopy"
$SIZE    = "arm-none-eabi-size"

# ---------------------------------------------------------------------------
# Source files
# ---------------------------------------------------------------------------
$SRCS = @(
    "main.c"
    "bsp\bsp_clock.c"
    "bsp\bsp_gpio.c"
    "bsp\bsp_i2c.c"
    "bsp\bsp_iwdg.c"
    "bsp\bsp_systick.c"
    "bsp\bsp_uart.c"
    "platform/stm32g0/startup_stm32g031xx.c"
    "drivers/sensor_sensirion_sen66/sensirion_sen66.c"
    "drivers/sensor_infwin_co/infwin_co_sensor.c"
    "middleware/modbus/modbus_crc.c"
    "middleware/modbus/modbus_slave.c"
)
# ---------------------------------------------------------------------------
# Build flags (as single strings — passed via cmd /c to avoid PS array issues)
# ---------------------------------------------------------------------------
$MCU_STR  = "-mcpu=cortex-m0plus -mthumb -mfloat-abi=soft"
$INC_STR  = "-I. -I`"$CMSIS_ROOT\Device\ST\STM32G0xx\Include`" -I`"$CMSIS_ROOT\Include`""
$DEF_STR  = "-DSTM32G031xx"
$OPT_STR  = "-std=c11 -Wall -Wextra -Wno-unused-parameter -ffunction-sections -fdata-sections -ffreestanding -fno-common -Os"

$CFLAGS_STR  = "$MCU_STR $INC_STR $DEF_STR $OPT_STR"
$LDFLAGS_STR = "$MCU_STR -T$LD_SCRIPT -Wl,--gc-sections -Wl,-Map=$BUILD\$TARGET.map -Wl,--print-memory-usage -nostartfiles -nostdlib -lc -lgcc"

# ---------------------------------------------------------------------------
# Helper: run a shell command string, exit on failure
# ---------------------------------------------------------------------------
function Invoke-Build {
    param([string]$CmdStr)
    $result = cmd /c "$CmdStr 2>&1"
    Write-Host $result
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[FAILED] Exit code: $LASTEXITCODE" -ForegroundColor Red
        exit $LASTEXITCODE
    }
}

# ---------------------------------------------------------------------------
# Clean
# ---------------------------------------------------------------------------
if ($Clean -and (Test-Path $BUILD)) {
    Write-Host "`n>> Cleaning $BUILD ..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $BUILD
    Write-Host "   Done." -ForegroundColor Yellow
}

New-Item -ItemType Directory -Force -Path $BUILD | Out-Null

# ---------------------------------------------------------------------------
# Compile each source file
# ---------------------------------------------------------------------------
Write-Host "`n>> Compiling..." -ForegroundColor Cyan
$ObjList = @()

foreach ($src in $SRCS) {
    # Build object path mirroring source structure under $BUILD
    $objRel  = ($src -replace '\\', '/') -replace '\.c$', '.o'
    $objPath = "$BUILD\$($objRel -replace '/', '\')"
    $objDir  = Split-Path $objPath -Parent
    New-Item -ItemType Directory -Force -Path $objDir | Out-Null

    Write-Host "   CC  $src" -ForegroundColor White
    Invoke-Build "$CC $CFLAGS_STR -c `"$src`" -o `"$objPath`""
    $ObjList += "`"$objPath`""
}

# ---------------------------------------------------------------------------
# Link
# ---------------------------------------------------------------------------
Write-Host "`n>> Linking..." -ForegroundColor Cyan
$ELF     = "$BUILD\$TARGET.elf"
$ObjArgs = $ObjList -join " "
Invoke-Build "$CC $LDFLAGS_STR $ObjArgs -o `"$ELF`""

# ---------------------------------------------------------------------------
# Binary outputs
# ---------------------------------------------------------------------------
Write-Host "`n>> Generating HEX / BIN..." -ForegroundColor Cyan
Invoke-Build "$OBJCOPY -O ihex   `"$ELF`" `"$BUILD\$TARGET.hex`""
Invoke-Build "$OBJCOPY -O binary `"$ELF`" `"$BUILD\$TARGET.bin`""

# ---------------------------------------------------------------------------
# Size report
# ---------------------------------------------------------------------------
Write-Host "`n>> Memory usage:" -ForegroundColor Cyan
Invoke-Build "$SIZE `"$ELF`""

# ---------------------------------------------------------------------------
# Flash via OpenOCD
# ---------------------------------------------------------------------------
if ($Flash) {
    Write-Host "`n>> Flashing via OpenOCD (ST-Link)..." -ForegroundColor Magenta
    $BIN = "$BUILD\$TARGET.bin"
    Invoke-Build "openocd -f interface/stlink.cfg -f target/stm32g0x.cfg -c `"program $BIN verify reset exit 0x08000000`""
    Write-Host "`n>> Flash complete!" -ForegroundColor Green
}

Write-Host "`n>> Build successful!" -ForegroundColor Green
Write-Host "   ELF : $ELF"
Write-Host "   HEX : $BUILD\$TARGET.hex"
Write-Host "   BIN : $BUILD\$TARGET.bin"
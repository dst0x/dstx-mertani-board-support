# ===========================================================================
# Makefile — SEN66 Modbus Gateway  (STM32G031F8P6)
# Toolchain : arm-none-eabi-gcc
# Debug tool : OpenOCD + ST-Link
# ===========================================================================

TARGET   := mertani_board_support
BUILD    := build

# ---------------------------------------------------------------------------
# Toolchain
# ---------------------------------------------------------------------------
CC      := arm-none-eabi-gcc
OBJCOPY := arm-none-eabi-objcopy
OBJDUMP := arm-none-eabi-objdump
SIZE    := arm-none-eabi-size

# ---------------------------------------------------------------------------
# MCU flags (Cortex-M0+)
# ---------------------------------------------------------------------------
MCU_FLAGS := -mcpu=cortex-m0plus \
             -mthumb            \
             -mfloat-abi=soft

# ---------------------------------------------------------------------------
# Source files
# ---------------------------------------------------------------------------
SRCS := main.c                        \
        bsp/bsp_clock.c               \
        bsp/bsp_flash.c               \
        bsp/bsp_gpio.c                \
        bsp/bsp_i2c.c                 \
        bsp/bsp_iwdg.c                \
        bsp/bsp_systick.c             \
        bsp/bsp_uart.c                \
        platform/stm32g0/startup_stm32g031xx.c     \
        drivers/sensor_sensirion_sen66/sensirion_sen66.c           \
        drivers/sensor_infwin_co/infwin_co_sensor.c       \
        drivers/sensor_pmsx003/pmsx003_sensor.c           \
        middleware/modbus/modbus_crc.c               \
        middleware/modbus/modbus_slave.c             \
        middleware/sensor_manager.c                  \
        middleware/uart_manager.c

OBJS := $(BUILD)/main.o                                           \
        $(BUILD)/bsp_bsp_clock.o                                  \
        $(BUILD)/bsp_bsp_flash.o                                  \
        $(BUILD)/bsp_bsp_gpio.o                                   \
        $(BUILD)/bsp_bsp_i2c.o                                    \
        $(BUILD)/bsp_bsp_iwdg.o                                   \
        $(BUILD)/bsp_bsp_systick.o                                \
        $(BUILD)/bsp_bsp_uart.o                                   \
        $(BUILD)/platform_stm32g0_startup_stm32g031xx.o           \
        $(BUILD)/drivers_sensor_sensirion_sen66_sensirion_sen66.o \
        $(BUILD)/drivers_sensor_infwin_co_infwin_co_sensor.o     \
        $(BUILD)/drivers_sensor_pmsx003_pmsx003_sensor.o          \
        $(BUILD)/middleware_modbus_modbus_crc.o                   \
        $(BUILD)/middleware_modbus_modbus_slave.o                 \
        $(BUILD)/middleware_sensor_manager.o                      \
        $(BUILD)/middleware_uart_manager.o

# ---------------------------------------------------------------------------
# Include paths
# ---------------------------------------------------------------------------
# Root of project = include root so files use "bsp/...", "drivers/...", etc.
INC := -I.

# CMSIS device headers — local copy inside project
INC += -ICMSIS/Device/ST/STM32G0xx/Include
INC += -ICMSIS/Include

# Preprocessor defines (use G031xx for LPUART1 support)
DEFS := -DSTM32G031xx

# ---------------------------------------------------------------------------
# Compiler flags
# ---------------------------------------------------------------------------
CFLAGS := $(MCU_FLAGS)               \
           $(INC)                    \
           $(DEFS)                   \
           -std=c11                  \
           -Wall -Wextra             \
           -Wno-unused-parameter     \
           -ffunction-sections       \
           -fdata-sections           \
           -ffreestanding            \
           -fno-common               \
           -Os

# ---------------------------------------------------------------------------
# Linker flags
# ---------------------------------------------------------------------------
LD_SCRIPT := STM32G031F8PX_FLASH.ld

LDFLAGS := $(MCU_FLAGS)                        \
            -T$(LD_SCRIPT)                     \
            -Wl,--gc-sections                  \
            -Wl,-Map=$(BUILD)/$(TARGET).map    \
            -Wl,--print-memory-usage           \
            -nostartfiles                      \
            -nostdlib                          \
            -lc -lgcc

# ---------------------------------------------------------------------------
# Targets
# ---------------------------------------------------------------------------
.PHONY: all clean flash dump size

all: $(BUILD)/$(TARGET).hex $(BUILD)/$(TARGET).bin
	@$(SIZE) $(BUILD)/$(TARGET).elf

# Link
$(BUILD)/$(TARGET).elf: $(OBJS)
	@echo "  LD   $@"
	@$(CC) $(LDFLAGS) -o $@ $^

# Compile C sources
$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "  CC   $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# Compile with flattened naming (replace / with _)
$(BUILD)/%_%.o: */%.c
	@mkdir -p $(BUILD)
	@echo "  CC   $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%_%_%.o: */*/%.c
	@mkdir -p $(BUILD)
	@echo "  CC   $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%_%_%_%.o: */*/*/%.c
	@mkdir -p $(BUILD)
	@echo "  CC   $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# Explicit rule for PMSX003 sensor
$(BUILD)/drivers_sensor_pmsx003_pmsx003_sensor.o: drivers/sensor_pmsx003/pmsx003_sensor.c
	@mkdir -p $(BUILD)
	@echo "  CC   $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# Binary outputs
$(BUILD)/$(TARGET).hex: $(BUILD)/$(TARGET).elf
	@echo "  HEX  $@"
	@$(OBJCOPY) -O ihex $< $@

$(BUILD)/$(TARGET).bin: $(BUILD)/$(TARGET).elf
	@echo "  BIN  $@"
	@$(OBJCOPY) -O binary $< $@

# Memory size report
size: $(BUILD)/$(TARGET).elf
	$(SIZE) $<

# Disassembly dump
dump: $(BUILD)/$(TARGET).elf
	$(OBJDUMP) -d -S $< > $(BUILD)/$(TARGET).lst
	@echo "Listing: $(BUILD)/$(TARGET).lst"

# Flash via OpenOCD (ST-Link)
flash: $(BUILD)/$(TARGET).bin
	openocd -f interface/stlink.cfg \
	        -f target/stm32g0x.cfg  \
	        -c "program $(BUILD)/$(TARGET).bin verify reset exit 0x08000000"

clean:
	@rm -rf $(BUILD)
	@echo "Cleaned."

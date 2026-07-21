# ===========================================================================
# Makefile — Mertani Board Support (STM32G031F8P6)
# Toolchain : arm-none-eabi-gcc
# Debug tool : OpenOCD + ST-Link
#
# Multi-app build: each entry in APPS is a standalone firmware image with
# its own entry point (app/<name>/main.c) and its own sensor/UART config.
# Common BSP/middleware sources are shared and rebuilt per app so each app
# gets its own set of compile-time defines.
#
#   make               -> list available apps
#   make <app>         -> build AND flash that app          (e.g. make sen66_co)
#   make <app>-build   -> build only, no flashing
#   make <app>-flash   -> flash the already-built binary
#   make <app>-size    -> print memory usage for that app
#   make <app>-clean   -> remove that app's build output
#   make all           -> build every app (no flashing)
#   make clean         -> remove build/ entirely
# ===========================================================================

BUILD := build

CC      := arm-none-eabi-gcc
OBJCOPY := arm-none-eabi-objcopy
OBJDUMP := arm-none-eabi-objdump
SIZE    := arm-none-eabi-size

MCU_FLAGS := -mcpu=cortex-m0plus \
             -mthumb            \
             -mfloat-abi=soft

COMMON_SRCS := bsp/bsp_clock.c                                 \
               bsp/bsp_flash.c                                 \
               bsp/bsp_gpio.c                                  \
               bsp/bsp_i2c.c                                   \
               bsp/bsp_iwdg.c                                  \
               bsp/bsp_systick.c                                \
               bsp/bsp_uart.c                                  \
               platform/stm32g0/startup_stm32g031xx.c          \
               middleware/modbus/modbus_crc.c                  \
               middleware/modbus/modbus_slave.c                \
               middleware/sensor_manager.c                     \
               middleware/uart_manager.c

APPS := mertani_aqms_oikn mertani_zoglab_pms100 mertani_sensirion_sen66

APP_SRCS_mertani_aqms_oikn := app/mertani_aqms_oikn/main.c \
                      drivers/sensor_sensirion_sen66/sensirion_sen66.c \
                      drivers/sensor_infwin_co/infwin_co_sensor.c
APP_DEFS_mertani_aqms_oikn := -DSENSOR_ENABLE_SEN66=1 -DSENSOR_ENABLE_INFWIN_CO=1 -DSENSOR_ENABLE_PMSX003=0 -DUSART1_MODE=0

APP_SRCS_mertani_zoglab_pms100 := app/mertani_zoglab_pms100/main.c \
                          drivers/sensor_sensirion_sen66/sensirion_sen66.c \
                          drivers/sensor_pmsx003/pmsx003_sensor.c
APP_DEFS_mertani_zoglab_pms100 := -DSENSOR_ENABLE_SEN66=1 -DSENSOR_ENABLE_INFWIN_CO=0 -DSENSOR_ENABLE_PMSX003=1 -DUSART1_MODE=3

APP_SRCS_mertani_sensirion_sen66 := app/mertani_sensirion_sen66/main.c \
                        drivers/sensor_sensirion_sen66/sensirion_sen66.c
APP_DEFS_mertani_sensirion_sen66 := -DSENSOR_ENABLE_SEN66=1 -DSENSOR_ENABLE_INFWIN_CO=0 -DSENSOR_ENABLE_PMSX003=0 -DUSART1_MODE=4

obj_of = $(BUILD)/$(1)/$(subst /,_,$(basename $(2))).o

INC := -I.
INC += -ICMSIS/Device/ST/STM32G0xx/Include
INC += -ICMSIS/Include

DEFS := -DSTM32G031xx

# ---------------------------------------------------------------------------
# Compiler flags
# ---------------------------------------------------------------------------
CFLAGS_BASE := $(MCU_FLAGS)               \
               $(INC)                    \
               $(DEFS)                   \
               -std=c11                  \
               -Wall -Wextra             \
               -Wno-unused-parameter     \
               -ffunction-sections       \
               -fdata-sections           \
               -ffreestanding            \
               -fno-common               \
               -Os                       \
               -MMD -MP

$(foreach app,$(APPS),$(eval CFLAGS_$(app) := $(CFLAGS_BASE) $(APP_DEFS_$(app))))

# ---------------------------------------------------------------------------
# Linker flags
# ---------------------------------------------------------------------------
LD_SCRIPT := STM32G031F8PX_FLASH.ld

LDFLAGS := $(MCU_FLAGS)                        \
            -T$(LD_SCRIPT)                     \
            -Wl,--gc-sections                  \
            -Wl,--print-memory-usage           \
            -nostartfiles                      \
            -nostdlib                          \
            -lc -lgcc

# ---------------------------------------------------------------------------
# Per-app source/object lists
# ---------------------------------------------------------------------------
$(foreach app,$(APPS),$(eval SRCS_$(app) := $(COMMON_SRCS) $(APP_SRCS_$(app))))
$(foreach app,$(APPS),$(eval OBJS_$(app) := $(foreach s,$(SRCS_$(app)),$(call obj_of,$(app),$(s)))))
$(foreach app,$(APPS),$(eval DEPS_$(app) := $$(OBJS_$(app):.o=.d)))

# One explicit compile rule per (app, source file) pair — avoids relying on
# pattern rules with more than one '%', which GNU Make does not support.
define COMPILE_RULE
$(call obj_of,$(1),$(2)): $(2)
	@mkdir -p $$(dir $$@)
	@echo "  CC   [$(1)] $$<"
	@$$(CC) $$(CFLAGS_$(1)) -c $$< -o $$@
endef
$(foreach app,$(APPS),$(foreach s,$(SRCS_$(app)),$(eval $(call COMPILE_RULE,$(app),$(s)))))

# ---------------------------------------------------------------------------
# Per-app build/flash/size/clean targets
# ---------------------------------------------------------------------------
define APP_RULES
$(BUILD)/$(1)/$(1).elf: $$(OBJS_$(1))
	@echo "  LD   $$@"
	@$$(CC) $(LDFLAGS) -Wl,-Map=$(BUILD)/$(1)/$(1).map -o $$@ $$^

$(BUILD)/$(1)/$(1).hex: $(BUILD)/$(1)/$(1).elf
	@echo "  HEX  $$@"
	@$$(OBJCOPY) -O ihex $$< $$@

$(BUILD)/$(1)/$(1).bin: $(BUILD)/$(1)/$(1).elf
	@echo "  BIN  $$@"
	@$$(OBJCOPY) -O binary $$< $$@

.PHONY: $(1) $(1)-build $(1)-flash $(1)-size $(1)-dump $(1)-clean

$(1)-build: $(BUILD)/$(1)/$(1).hex $(BUILD)/$(1)/$(1).bin
	@$$(SIZE) $(BUILD)/$(1)/$(1).elf

$(1)-flash: $(BUILD)/$(1)/$(1).bin
	openocd -f interface/stlink.cfg \
	        -f target/stm32g0x.cfg  \
	        -c "program $(BUILD)/$(1)/$(1).bin verify reset exit 0x08000000"

$(1): $(1)-build $(1)-flash

$(1)-size: $(BUILD)/$(1)/$(1).elf
	$$(SIZE) $$<

$(1)-dump: $(BUILD)/$(1)/$(1).elf
	$$(OBJDUMP) -d -S $$< > $(BUILD)/$(1)/$(1).lst
	@echo "Listing: $(BUILD)/$(1)/$(1).lst"

$(1)-clean:
	@rm -rf $(BUILD)/$(1)
	@echo "Cleaned $(1)."
endef
$(foreach app,$(APPS),$(eval $(call APP_RULES,$(app))))

# ---------------------------------------------------------------------------
# Top-level targets
# ---------------------------------------------------------------------------
.PHONY: all list-apps clean

.DEFAULT_GOAL := list-apps

list-apps:
	@echo "Available apps:"
	@for a in $(APPS); do echo "  make $$a        - build + flash"; done
	@echo ""
	@echo "Also: make <app>-build | <app>-flash | <app>-size | <app>-dump | <app>-clean"
	@echo "      make all (build every app, no flash) | make clean"

all: $(foreach app,$(APPS),$(app)-build)

clean:
	@rm -rf $(BUILD)
	@echo "Cleaned."

-include $(foreach app,$(APPS),$(DEPS_$(app)))

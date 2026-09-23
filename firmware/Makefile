##########################################################################################################################
# boot_seq_c8t6 - STM32F103C8T6 上电时序控制器
# GNU Arm Embedded (STM32CubeIDE 自带) + Makefile 构建
##########################################################################################################################

######################################
# target
######################################
TARGET = boot_seq_c8t6

######################################
# building variables
######################################
# debug build?
DEBUG = 1
# optimization
OPT = -Og

#######################################
# paths
#######################################
BUILD_DIR = build

######################################
# source
######################################
# C sources
C_SOURCES =  \
Core/Src/main.c \
Core/Src/cmd.c \
Core/Src/led.c \
Core/Src/pwr_seq.c \
Core/Src/stm32f10x_it.c \
Core/Src/tick.c \
Core/Src/uart.c \
Drivers/CMSIS/system_stm32f10x.c \
Drivers/StdPeriph/src/misc.c \
Drivers/StdPeriph/src/stm32f10x_exti.c \
Drivers/StdPeriph/src/stm32f10x_gpio.c \
Drivers/StdPeriph/src/stm32f10x_rcc.c \
Drivers/StdPeriph/src/stm32f10x_tim.c \
Drivers/StdPeriph/src/stm32f10x_usart.c

# ASM sources (GCC 版启动文件)
ASM_SOURCES =  \
startup_stm32f10x_md.s

#######################################
# binaries
#######################################
PREFIX = arm-none-eabi-
# The gcc compiler bin path can be either defined in make command via GCC_PATH variable (> make GCC_PATH=xxx)
# either it can be added to the PATH environment variable.
ifdef GCC_PATH
CC = $(GCC_PATH)/$(PREFIX)gcc
AS = $(GCC_PATH)/$(PREFIX)gcc -x assembler-with-cpp
CP = $(GCC_PATH)/$(PREFIX)objcopy
SZ = $(GCC_PATH)/$(PREFIX)size
else
CC = $(PREFIX)gcc
AS = $(PREFIX)gcc -x assembler-with-cpp
CP = $(PREFIX)objcopy
SZ = $(PREFIX)size
endif
HEX = $(CP) -O ihex
BIN = $(CP) -O binary -S

#######################################
# CFLAGS
#######################################
# cpu (Cortex-M3, 无 FPU)
CPU = -mcpu=cortex-m3
# mcu
MCU = $(CPU) -mthumb

# C defines
#   STM32F10X_MD        : 中容量器件 (C8T6), 决定 IRQ 向量/寄存器范围
#   USE_STDPERIPH_DRIVER: 启用标准外设库 (stm32f10x.h -> stm32f10x_conf.h)
C_DEFS =  \
-DUSE_STDPERIPH_DRIVER \
-DSTM32F10X_MD

# includes
C_INCLUDES =  \
-ICore/Inc \
-IDrivers/CMSIS \
-IDrivers/StdPeriph/inc

# compile gcc flags
ASFLAGS = $(MCU) $(AS_INCLUDES) $(OPT) -Wall -fdata-sections -ffunction-sections

CFLAGS += $(MCU) $(C_DEFS) $(C_INCLUDES) $(OPT) -Wall -fdata-sections -ffunction-sections

ifeq ($(DEBUG), 1)
CFLAGS += -g -gdwarf-2
endif

# Generate dependency information
CFLAGS += -MMD -MP -MF"$(@:%.o=%.d)"


#######################################
# LDFLAGS
#######################################
# link script
LDSCRIPT = STM32F103C8Tx_FLASH.ld

# libraries
LIBS = -lc -lm -lnosys
LIBDIR =
LDFLAGS = $(MCU) -specs=nano.specs -T$(LDSCRIPT) $(LIBDIR) $(LIBS) -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref -Wl,--gc-sections

# default action: build all
all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/$(TARGET).bin


#######################################
# build the application
#######################################
# list of objects
OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))

# list of ASM program objects
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))

$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.s Makefile | $(BUILD_DIR)
	$(AS) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) Makefile
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	$(SZ) $@

$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(HEX) $< $@

$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(BIN) $< $@

$(BUILD_DIR):
	mkdir $@


#######################################
# clean up
#######################################
clean:
	-rm -fR $(BUILD_DIR)

#######################################
# dependencies
#######################################
-include $(wildcard $(BUILD_DIR)/*.d)

# *** EOF ***

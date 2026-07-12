# Define directories
ROOT_DIR  := ..
BUILD_DIR := $(ROOT_DIR)/build/fw

# Toolchain
CC      = riscv64-unknown-elf-gcc
OBJCPY  = riscv64-unknown-elf-objcopy
OBJDUMP = riscv64-unknown-elf-objdump
MAP     = riscv64-unknown-elf-nm

# Flags
OPTIMIZE = -O2
CFLAGS   = -g -Wall -nostdlib -march=rv32imav -mabi=ilp32 $(OPTIMIZE) -I$(ROOT_DIR)/fw/
CFLAGS  += -Wl,-Map=$(BUILD_DIR)/program.map

OBJFLAGS = -O verilog --verilog-data-width=4 -S

# Sources
LSCRIPT   = bootstrap/lscript.ld
BOOTSTRAP = bootstrap/boot.S
C_SRC     = main.c

# Output Artifacts
BOOT_OBJ = $(BUILD_DIR)/boot.o
C_OBJ    = $(BUILD_DIR)/main.o
ELF      = $(BUILD_DIR)/program.elf
TARGET   = $(BUILD_DIR)/output.vh
DUMP     = $(BUILD_DIR)/program.dump

# Default target
all: $(BUILD_DIR) $(TARGET) dump

# Ensure the build directory exists
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Generate the Verilog Hex file (.vh)
$(TARGET): $(ELF)
	$(OBJCPY) $(OBJFLAGS) $< $@

# Link the objects into an ELF
$(ELF): $(BOOT_OBJ) $(C_OBJ) $(LSCRIPT)
	$(CC) $(CFLAGS) -T $(LSCRIPT) $(BOOT_OBJ) $(C_OBJ) -o $@

# Compile the assembly bootloader into an object
$(BUILD_DIR)/boot.o: $(BOOTSTRAP)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile the C source into an object
$(BUILD_DIR)/main.o: $(C_SRC)
	$(CC) $(CFLAGS) -c $< -o $@

dump: $(DUMP)

map: $(ELF)
	$(MAP) $(ELF)

$(DUMP): $(ELF)
	$(OBJDUMP) -dS $< > $@

.PHONY: clean dump all

clean:
	-rm -rf $(BUILD_DIR)

# Define directories
ROOT_DIR  := ..
BUILD_DIR := $(ROOT_DIR)/build/fw

# Toolchain
CC      = riscv64-unknown-elf-gcc
OBJCPY  = riscv64-unknown-elf-objcopy
OBJDUMP = riscv64-unknown-elf-objdump

# Flags
OPTIMIZE = -O0
CFLAGS   = -Wall -nostdlib -march=rv32imav -mabi=ilp32 $(OPTIMIZE)
CFLAGS  += -Wl,-Map=$(BUILD_DIR)/program.map

OBJFLAGS = -O verilog --verilog-data-width=4 -S

# Sources
LSCRIPT   = bootstrap/lscript.ld
BOOTSTRAP = bootstrap/boot.S
SRC       = main.S

# Output Artifacts
BOOT_OBJ = $(BUILD_DIR)/boot.o
SRC_OBJ  = $(BUILD_DIR)/main.o
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
$(ELF): $(BOOT_OBJ) $(SRC_OBJ) $(LSCRIPT)
	$(CC) $(CFLAGS) -T $(LSCRIPT) $(BOOT_OBJ) $(SRC_OBJ) -o $@

# Compile the assembly files into objects
$(BUILD_DIR)/boot.o: $(BOOTSTRAP)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/main.o: $(SRC)
	$(CC) $(CFLAGS) -c $< -o $@


dump: $(DUMP)

$(DUMP): $(ELF)
	$(OBJDUMP) -d $< > $@

.PHONY: clean dump all

clean:
	-rm -rf $(BUILD_DIR)

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
CFLAGS   = -g -Wall -nostdlib -march=rv32im -mabi=ilp32 $(OPTIMIZE) -I$(ROOT_DIR)/fw/
CFLAGS  += -Wl,-Map=$(BUILD_DIR)/program.map
CFLAGS  += -ffixed-x21 -ffixed-x22 -ffixed-x23 -ffixed-x24 -ffixed-x25 -ffixed-x26 -ffixed-x27 -ffixed-x28 -ffixed-x29 -ffixed-x30 -ffixed-x31

OBJFLAGS = -O verilog --verilog-data-width=4 -S

# Sources
LSCRIPT   = bootstrap/lscript.ld
BOOTSTRAP = bootstrap/boot.S
C_SRC     = main.c

# Output Artifacts
BOOT_OBJ = $(BUILD_DIR)/boot.o
C_OBJ    = $(BUILD_DIR)/main.o
ELF      = $(BUILD_DIR)/program.elf
# TARGET   = $(BUILD_DIR)/output.vh
DUMP     = $(BUILD_DIR)/program.dump
IMEM_VH  = $(BUILD_DIR)/imem.vh
DMEM_VH  = $(BUILD_DIR)/dmem.vh

# Default target
all: $(BUILD_DIR) $(IMEM_VH) $(DMEM_VH) dump

# Ensure the build directory exists
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(IMEM_VH): $(ELF)
	$(OBJCPY) $(OBJFLAGS) -j .text $< $@

$(DMEM_VH): $(ELF)
	$(OBJCPY) $(OBJFLAGS) -j .data -j .rodata -j .sdata $< $@

# Link the objects into an ELF
$(ELF): $(BOOT_OBJ) $(C_OBJ) $(LSCRIPT)
	$(CC) $(CFLAGS) -Wl,--no-check-sections -T $(LSCRIPT) $(BOOT_OBJ) $(C_OBJ) -o $@

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

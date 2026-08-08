# Define directories
ROOT_DIR  := ..
BUILD_DIR := $(ROOT_DIR)/build/fw
LLVM_BIN  := $(ROOT_DIR)/extern/llvm-project/build/bin

# Toolchain
# CC      = riscv64-unknown-elf-gcc
OBJCPY  = riscv64-unknown-elf-objcopy
# OBJDUMP = riscv64-unknown-elf-objdump
# MAP     = riscv64-unknown-elf-nm

CC      := $(LLVM_BIN)/clang
# OBJCPY  := $(LLVM_BIN)/llvm-objcopy
OBJDUMP := $(LLVM_BIN)/llvm-objdump
MAP     := $(LLVM_BIN)/llvm-nm
SIZE    := $(LLVM_BIN)/llvm-size

# Flags
OPTIMIZE := -O2
CFLAGS   := -g -Wall -nostdlib -target riscv32-unknown-elf -march=rv32im_zbb_xzkp256b -mabi=ilp32 $(OPTIMIZE) -I$(ROOT_DIR)/fw/
CFLAGS   += -mllvm -riscv-xzkp-max-bregs=19
CFLAGS   += -MMD -MP
LDFLAGS  = -Wl,-Map=$(BUILD_DIR)/program.map -Wl,--no-check-sections -T $(LSCRIPT)

OBJFLAGS := -O verilog --verilog-data-width=4 -S

# Sources
LSCRIPT   := bootstrap/lscript.ld
BOOTSTRAP := bootstrap/boot.S
C_SRC     := main.c

# Output Artifacts
BOOT_OBJ := $(BUILD_DIR)/boot.o
C_OBJ    := $(BUILD_DIR)/main.o
ELF      := $(BUILD_DIR)/program.elf
# TARGET   = $(BUILD_DIR)/output.vh
DUMP     := $(BUILD_DIR)/program.dump
IMEM_VH  := $(BUILD_DIR)/imem.vh
DMEM_VH  := $(BUILD_DIR)/dmem.vh

# Auto-generated header dependency files
DEPS := $(BOOT_OBJ:.o=.d) $(C_OBJ:.o=.d)

# Default target
all: $(IMEM_VH) $(DMEM_VH) dump

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(IMEM_VH): $(ELF)
	$(OBJCPY) $(OBJFLAGS) -j .text $< $@

$(DMEM_VH): $(ELF)
	$(OBJCPY) $(OBJFLAGS) -j .data -j .rodata -j .sdata $< $@

PARAMS_FILE = $(BUILD_DIR)/fw_params.txt

$(ELF): $(BOOT_OBJ) $(C_OBJ) $(LSCRIPT)
	$(CC) $(CFLAGS) $(LDFLAGS) $(BOOT_OBJ) $(C_OBJ) -o $@
	@echo "Extracting dynamic memory sizes..."
	@mkdir -p $(dir $(PARAMS_FILE))
	@inputs_hex=$$( $(MAP) $@ | grep __inputs_base | awk '{print $$1}' ); \
	dmem_hex=$$( $(MAP) $@ | grep __dmem_total_size | awk '{print $$1}' ); \
	dmem_dec=$$( printf "%d" 0x$$dmem_hex ); \
	echo "DMEM_SIZE_DEC=$$dmem_dec" > $(PARAMS_FILE); \
	echo "INPUTS_BASE_HEX=$$inputs_hex" >> $(PARAMS_FILE)
	@echo "Memory parameters written to $(PARAMS_FILE)"

# Compile the assembly bootloader into an object
$(BOOT_OBJ): $(BOOTSTRAP) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile the C source into an object
$(C_OBJ): $(C_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

dump: $(DUMP)

map: $(ELF)
	@$(SIZE) $(ELF)
	@echo ""
	@$(MAP) $(ELF)

$(DUMP): $(ELF)
	$(OBJDUMP) --mattr=+zbb,+xzkp256b -dS $< > $@

.PHONY: clean dump all map

clean:
	-rm -rf $(BUILD_DIR)

-include $(DEPS)

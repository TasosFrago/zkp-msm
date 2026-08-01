# ==============================================================================
# Directory Setup
# ==============================================================================
ROOT_DIR  := ../..
BUILD_DIR := $(ROOT_DIR)/build/sim/zbp
RTL_DIR   := $(ROOT_DIR)/rtl
SIM_DIR   := .
FW_DIR    := $(ROOT_DIR)/fw
FW_PARAMS := $(ROOT_DIR)/build/fw/fw_params.txt

VERILATOR := verilator
CXX       := clang++
DEBUG     ?= 1
TOP_MOD   := tb_top

# ==============================================================================
# Source Files
# ==============================================================================
HDL_SRCS  := $(RTL_DIR)/zbp/zbp_pkg.sv
HDL_SRCS  += $(SIM_DIR)/dbg_pkg.sv
HDL_SRCS  += $(SIM_DIR)/dbg_fetch_trace_if.sv
HDL_SRCS  += $(SIM_DIR)/tb_top.sv
HDL_SRCS  += $(filter-out $(RTL_DIR)/zbp/zbp_pkg.sv, $(wildcard $(RTL_DIR)/zbp/*.sv))
HDL_SRCS  += $(wildcard $(RTL_DIR)/mm/*.sv)
HDL_SRCS  += $(wildcard $(RTL_DIR)/adder/*.sv)

TB_SRC    := $(SIM_DIR)/zbp_core_tb.cpp $(SIM_DIR)/disasm.cpp

# ==============================================================================
# Compiler & Verilator Flags
# ==============================================================================
LLVM_CONFIG   := $(ROOT_DIR)/extern/llvm-project/build/bin/llvm-config
LLVM_CXXFLAGS := $(filter-out -std=% -fno-exceptions -fno-rtti,$(shell $(LLVM_CONFIG) --cxxflags))

LLVM_LIB_DIR := $(abspath $(ROOT_DIR)/extern/llvm-project/build/lib)

LLVM_LDFLAGS := -L$(LLVM_LIB_DIR) \
                -Wl,--start-group \
                -lLLVMRISCVDisassembler \
                -lLLVMRISCVDesc \
                -lLLVMRISCVInfo \
                -lLLVMMCDisassembler \
                -lLLVMMC \
                -lLLVMBinaryFormat \
                -lLLVMDebugInfoDWARF \
                -lLLVMDebugInfoDWARFLowLevel \
                -lLLVMCore \
                -lLLVMTargetParser \
                -lLLVMSupport \
                -lLLVMDemangle \
                -Wl,--end-group \
                -lz -lzstd -ltinfo

V_CXXFLAGS := -std=c++23 -Wall -I$(abspath $(ROOT_DIR)/src) -I$(abspath $(ROOT_DIR)/sim/utils/) $(LLVM_CXXFLAGS)

# 3. Explicitly set V_LDFLAGS to include the LLVM flags
V_LDFLAGS  := $(LLVM_LDFLAGS)

ifeq ($(DEBUG), 1)
V_CXXFLAGS += -ggdb -fsanitize=address -fno-omit-frame-pointer -O0
V_LDFLAGS  += -fsanitize=address
$(info [INFO]: SIM Debug mode ON)
else
V_CXXFLAGS += -O3 -DNDEBUG
$(info [INFO]: SIM Debug mode OFF)
endif

V_FLAGS := --cc --exe --build --timing -j $(shell nproc)
V_FLAGS += --trace-fst --trace-structs --trace-max-array 128
V_FLAGS += --Mdir $(abspath $(BUILD_DIR))
V_FLAGS += --top-module $(TOP_MOD)
V_FLAGS += -CFLAGS "$(V_CXXFLAGS)" -LDFLAGS "$(V_LDFLAGS)"
V_FLAGS += --compiler clang
V_FLAGS += -MAKEFLAGS "CXX=$(CXX) LINK=$(CXX) -s"

# ==============================================================================
# Firmware Targets
# ==============================================================================
FW_TARGET := $(ROOT_DIR)/build/fw/output.vh
LIBDISASM := $(BUILD_DIR)/libdisasm.so

TIME_FMT := \n=== SIMULATION TIME ===\nElapsed Time : %E\nCPU Usage    : %P\nMax Memory   : %M KB\n=======================

.PHONY: all sim run clean fw compdb libdisasm

all: run

fw: $(FW_TARGET)

$(FW_TARGET):
	@echo -e "\n\033[0;34m[FW] Firmware missing or out-of-date. Compiling...\033[0m"
	@$(MAKE) -C $(FW_DIR)

# ==============================================================================
# Hardware Simulation Targets
# ==============================================================================
sim: $(BUILD_DIR)/V$(TOP_MOD)

$(BUILD_DIR)/V$(TOP_MOD): $(HDL_SRCS) $(TB_SRC) $(FW_TARGET)
	@echo -e "\n\033[0;32m[VERILATOR] Building Simulation: V$(TOP_MOD)\033[0m"
	@mkdir -p $(BUILD_DIR)

	@echo "FW_PARAMS = $(FW_PARAMS)"
	@DMEM_SIZE=$$(awk -F'=' '/DMEM_SIZE_DEC/ {print $$2}' $(FW_PARAMS)); \
	INPUTS_ADDR=$$(awk -F'=' '/INPUTS_BASE_HEX/ {print $$2}' $(FW_PARAMS)); \
	echo -e "\033[0;36m[VERILATOR] Injecting DMEM_SIZE_BYTES=$$DMEM_SIZE \033[0m"; \
	CXX=$(CXX) $(VERILATOR) $(V_FLAGS) \
		-GMEM_SIZE_BYTES=$$DMEM_SIZE \
		$(HDL_SRCS) $(TB_SRC)

run: sim
	@echo -e "\n\033[0;32m[SIM] Running $(TOP_MOD) testbench...\033[0m"
	@mkdir -p $(BUILD_DIR)/traces
	@mkdir -p $(BUILD_DIR)/dbg_logs
	@cd $(BUILD_DIR) && /usr/bin/time -f "$(TIME_FMT)" ./V$(TOP_MOD) \
		+MEM_FILE=$(abspath $(FW_TARGET)) \
		+NO_FETCH_TRACE

	@echo -e "\n\033[0;36m[SIM] Checking Register Dump (First 16 lines):\033[0m"
	@head -n 16 $(BUILD_DIR)/scalar_regs_dump.txt || true

compdb:
	bear --output compile_commands.json -- $(MAKE) sim --always-make

libdisasm: $(LIBDISASM)
$(LIBDISASM): $(SIM_DIR)/disasm.cpp
	@echo -e "\n\033[0;32m[BUILD] Compiling C++ Disassembler to Shared Library: $@\033[0m"
	@$(CXX) -shared -fPIC -std=c++23 \
		-I$(abspath $(ROOT_DIR)/src) -I$(abspath $(ROOT_DIR)/sim/utils/) \
		$(LLVM_CXXFLAGS) \
		$< -o $@ \
		$(LLVM_LDFLAGS)

wave:
	@echo "Starting surfer waveform..."
	@RUST_LOG=off surfer --state-file $(SIM_DIR)/state.surf.ron $(BUILD_DIR)/dump.fst &

clean:
	@echo -e "[CLEAN] Removing $(BUILD_DIR) and FW build..."
	@rm -rf $(BUILD_DIR)
	@$(MAKE) -C $(FW_DIR) clean

# ==============================================================================
# Directory Setup
# ==============================================================================
ROOT_DIR  := ../..
BUILD_DIR := $(ROOT_DIR)/build/sim/zbp
RTL_DIR   := $(ROOT_DIR)/rtl
SIM_DIR   := .
FW_DIR    := $(ROOT_DIR)/fw

VERILATOR := verilator
CXX       := clang++
DEBUG     ?= 1
TOP_MOD   := tb_top

# ==============================================================================
# Source Files
# ==============================================================================
HDL_SRCS  := $(RTL_DIR)/zbp/zbp_pkg.sv
HDL_SRCS  += $(SIM_DIR)/tb_top.sv
HDL_SRCS  += $(filter-out $(RTL_DIR)/zbp/zbp_pkg.sv, $(wildcard $(RTL_DIR)/zbp/*.sv))

TB_SRC    := $(SIM_DIR)/zbp_core_tb.cpp

# ==============================================================================
# Compiler & Verilator Flags
# ==============================================================================
V_CXXFLAGS := -std=c++23 -Wall
V_LDFLAGS :=

ifeq ($(DEBUG), 1)
V_CXXFLAGS += -ggdb -fsanitize=address -fno-omit-frame-pointer -O0
V_LDFLAGS  += -fsanitize=address
$(info [INFO]: SIM Debug mode ON)
else
V_CXXFLAGS += -O3 -DNDEBUG
$(info [INFO]: SIM Debug mode OFF)
endif

V_FLAGS := --cc --exe --build -j $(shell nproc)
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

.PHONY: all sim run clean fw compdb

all: run

fw: $(FW_TARGET)

$(FW_TARGET):
	@echo -e "\n\033[0;34m[FW] Firmware missing or out-of-date. Compiling...\033[0m"
	@$(MAKE) -C $(FW_DIR)

# ==============================================================================
# Hardware Simulation Targets
# ==============================================================================
sim: $(BUILD_DIR)/V$(TOP_MOD)

# The Verilator executable only depends on hardware files (RTL and C++)
$(BUILD_DIR)/V$(TOP_MOD): $(HDL_SRCS) $(TB_SRC)
	@echo -e "\n\033[0;32m[VERILATOR] Building Simulation: V$(TOP_MOD)\033[0m"
	@mkdir -p $(BUILD_DIR)
	@CXX=$(CXX) $(VERILATOR) $(V_FLAGS) $(HDL_SRCS) $(TB_SRC)

# 'run' depends on BOTH the compiled hardware AND the compiled firmware
run: sim $(FW_TARGET)
	@echo -e "\n\033[0;32m[SIM] Running $(TOP_MOD) testbench...\033[0m"
	@cd $(BUILD_DIR) && ./V$(TOP_MOD) +MEM_FILE=$(abspath $(FW_TARGET))
	@echo -e "\n\033[0;36m[SIM] Checking Register Dump (First 16 lines):\033[0m"
	@head -n 16 $(BUILD_DIR)/scalar_regs_dump.txt || true

compdb:
	bear --output compile_commands.json -- $(MAKE) sim --always-make

wave:
	@echo "Starting surfer waveform..."
	@RUST_LOG=off surfer --state-file $(SIM_DIR)/state.surf.ron $(BUILD_DIR)/dump.fst &

clean:
	@echo -e "[CLEAN] Removing $(BUILD_DIR) and FW build..."
	@rm -rf $(BUILD_DIR)
	@$(MAKE) -C $(FW_DIR) clean

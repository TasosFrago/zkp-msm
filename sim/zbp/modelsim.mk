# ==============================================================================
# Directory Setup
# ==============================================================================
ROOT_DIR  := ../..
BUILD_DIR := $(ROOT_DIR)/build/sim/zbp_questa
RTL_DIR   := $(ROOT_DIR)/rtl
SIM_DIR   := .
FW_DIR    := $(ROOT_DIR)/fw
FW_TARGET := $(ROOT_DIR)/build/fw/output.vh

TOP_MOD   := zbp_tb

# ==============================================================================
# Toolchain Setup
# ==============================================================================
MTI_HOME  ?= /opt/intelFPGA/20.1/modelsim_ase

VLIB      := vlib
VMAP      := vmap
VLOG      := vlog
VSIM      := vsim
CXX       := g++

ARCH_FLAG := -m32

CXXFLAGS  := $(ARCH_FLAG) -fPIC -shared -I$(MTI_HOME)/include -lcapstone

# ==============================================================================
# Source Files (Order Matters for SystemVerilog Packages)
# ==============================================================================
HDL_SRCS  := $(RTL_DIR)/zbp/zbp_pkg.sv
HDL_SRCS  += $(SIM_DIR)/dbg_pkg.sv
HDL_SRCS  += $(SIM_DIR)/dbg_fetch_trace_if.sv
HDL_SRCS  += $(SIM_DIR)/tb_top.sv
HDL_SRCS  += $(filter-out $(RTL_DIR)/zbp/zbp_pkg.sv, $(wildcard $(RTL_DIR)/zbp/*.sv))
HDL_SRCS  += $(wildcard $(RTL_DIR)/mm/*.sv)
HDL_SRCS  += $(SIM_DIR)/zbp_tb.sv

# 4. DPI C++ Code
DPI_SRC   := $(SIM_DIR)/disasm.cpp
DPI_LIB   := $(BUILD_DIR)/disasm.so

ABS_HDL_SRCS := $(foreach f,$(HDL_SRCS),$(abspath $(f)))

# ==============================================================================
# Targets
# ==============================================================================
.PHONY: all run gui fw dpi compile clean

all: run

# 1. Compile Firmware
fw: $(FW_TARGET)

$(FW_TARGET):
	@echo -e "\n\033[0;34m[FW] Compiling Firmware...\033[0m"
	@echo $(FW_DIR)
	@$(MAKE) -C $(FW_DIR)

dpi: $(DPI_LIB)

$(DPI_LIB): $(DPI_SRC)
	@echo -e "\n\033[0;35m[DPI] Compiling C++ Shared Library...\033[0m"
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $<

compile: fw dpi
	@echo -e "\n\033[0;32m[VLOG] Compiling SystemVerilog Sources...\033[0m"
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && $(VLIB) work
	@cd $(BUILD_DIR) && $(VMAP) work work
	@cd $(BUILD_DIR) && $(VLOG) -work work -sv $(ABS_HDL_SRCS)

run: compile
	@echo -e "\n\033[0;36m[VSIM] Running Simulation (Terminal Mode)...\033[0m"
	@cd $(BUILD_DIR) && $(VSIM) -c -work work -sv_lib disasm $(TOP_MOD) \
		+MEM_FILE=$(abspath $(FW_TARGET)) \
		-do "vcd file dump.vcd; add wave -r /*; run -all; vcd flush; quit"

gui: compile
	@echo -e "\n\033[0;36m[VSIM] Running Simulation (GUI Mode)...\033[0m"
	@cd $(BUILD_DIR) && $(VSIM) -work work -sv_lib disasm $(TOP_MOD) \
		+MEM_FILE=$(abspath $(FW_TARGET)) \
		-do "add wave -r /*; run -all;"

clean:
	@echo -e "\n[CLEAN] Removing Questa build directory..."
	@rm -rf $(BUILD_DIR)

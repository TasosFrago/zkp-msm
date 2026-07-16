# ==============================================================================
# Directory Setup
# ==============================================================================
ROOT_DIR  := ../..
BUILD_DIR := $(ROOT_DIR)/build/sim/zbp_vivado
RTL_DIR   := $(ROOT_DIR)/rtl
SIM_DIR   := .
FW_DIR    := $(ROOT_DIR)/fw
FW_TARGET := $(ROOT_DIR)/build/fw/output.vh

TOP_MOD   := zbp_tb
SNAPSHOT  := $(TOP_MOD)_sim

# ==============================================================================
# Toolchain Setup
# ==============================================================================
XVLOG := xvlog
XELAB := xelab
XSIM  := xsim
CXX   := g++

VIVADO_INCLUDE ?= $(XILINX_VIVADO)/data/xsim/include

CXXFLAGS := -m64 -fPIC -std=c++23 -g -shared -static-libgcc -static-libstdc++ -I$(VIVADO_INCLUDE) -I$(abspath $(ROOT_DIR)/src) -lcapstone

# ==============================================================================
# Source Files (Order Matters for SystemVerilog Packages)
# ==============================================================================
PKG_SRCS  := $(RTL_DIR)/zbp/zbp_pkg.sv \
             $(RTL_DIR)/zbp/isa_pkg.sv \
             $(SIM_DIR)/dbg_pkg.sv

HDL_SRCS  := $(filter-out $(PKG_SRCS), $(wildcard $(RTL_DIR)/zbp/*.sv))
HDL_SRCS  += $(wildcard $(RTL_DIR)/mm/*.sv)
HDL_SRCS  += $(wildcard $(RTL_DIR)/adder/*.sv)
HDL_SRCS  += $(SIM_DIR)/dbg_fetch_trace_if.sv

# 3. Testbench
TB_SRCS   := $(SIM_DIR)/tb_top.sv \
             $(SIM_DIR)/zbp_tb.sv

# 4. DPI C++ Code
DPI_SRC   := $(SIM_DIR)/tb_dpi.cpp $(SIM_DIR)/disasm.cpp
DPI_LIB   := $(BUILD_DIR)/libdpi.so

ABS_PKG := $(foreach f,$(PKG_SRCS),$(abspath $(f)))
ABS_HDL := $(foreach f,$(HDL_SRCS),$(abspath $(f)))
ABS_TB  := $(foreach f,$(TB_SRCS),$(abspath $(f)))

# ==============================================================================
# Targets
# ==============================================================================
.PHONY: all run gui fw dpi compile clean

all: run

# 1. Compile Firmware
fw: $(FW_TARGET)

$(FW_TARGET):
	@echo -e "\n\033[0;34m[FW] Compiling Firmware...\033[0m"
	@$(MAKE) -C $(FW_DIR)

# 2. Compile 64-bit DPI C++ Library
dpi: $(DPI_LIB)

$(DPI_LIB): $(DPI_SRC)
	@echo -e "\n\033[0;35m[DPI] Compiling 64-bit C++ Shared Library...\033[0m"
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^

# 3. Compile & Elaborate SystemVerilog
compile: fw dpi
	@echo -e "\n\033[0;32m[XVLOG] Compiling SystemVerilog Sources...\033[0m"
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && $(XVLOG) -sv $(ABS_PKG) $(ABS_HDL) $(ABS_TB)
	@echo -e "\n\033[0;33m[XELAB] Elaborating Design and Linking DPI...\033[0m"
	@cd $(BUILD_DIR) && $(XELAB) -debug typical -timescale 1ns/1ps -sv_root $(abspath $(BUILD_DIR)) -sv_lib libdpi $(TOP_MOD) -s $(SNAPSHOT)

# 4. Run Simulation (CLI Mode)
run: compile
	@echo -e "\n\033[0;36m[XSIM] Running Simulation (Terminal Mode)...\033[0m"
	@echo "log_wave -r /; run all; exit" > $(BUILD_DIR)/run.tcl
	@mkdir -p $(BUILD_DIR)/traces
	@cd $(BUILD_DIR) && $(XSIM) $(SNAPSHOT) -tclbatch run.tcl -testplusarg MEM_FILE=$(abspath $(FW_TARGET))

# 5. Run Simulation (GUI Mode)
gui: compile
	@echo -e "\n\033[0;36m[XSIM] Running Simulation (GUI Mode)...\033[0m"
	@echo "log_wave -r /; run all" > $(BUILD_DIR)/gui.tcl
	@mkdir -p $(BUILD_DIR)/traces
	@cd $(BUILD_DIR) && $(XSIM) $(SNAPSHOT) -gui -tclbatch gui.tcl -testplusarg MEM_FILE=$(abspath $(FW_TARGET))

clean:
	@echo -e "\n[CLEAN] Removing Vivado build directory..."
	@rm -rf $(BUILD_DIR)

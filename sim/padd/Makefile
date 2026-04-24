ROOT_DIR := ../..
BUILD_DIR := $(ROOT_DIR)/build/sim/padd
RTL_DIR := $(ROOT_DIR)/rtl
SRC_DIR := $(ROOT_DIR)/src

VERILATOR = verilator
CXX = clang++

DEBUG ?= 1

TOP_MOD := padd

HDL_SRCS := $(wildcard $(RTL_DIR)/*.sv)

HDL_SRCS := $(RTL_DIR)/mm/pe_rightmost.sv \
	    $(RTL_DIR)/mm/pe_standard.sv \
	    $(RTL_DIR)/mm/systolic_array.sv \
	    $(RTL_DIR)/mm/cmul.sv \
	    $(RTL_DIR)/mm/mmul.sv \
	    $(RTL_DIR)/adder/mod_add.sv \
	    $(RTL_DIR)/padd/instr_pkg.sv \
	    $(RTL_DIR)/padd/regfile.sv \
	    $(RTL_DIR)/padd/alu.sv \
	    $(RTL_DIR)/padd/padd.sv

TB_SRC := padd_tb.cpp
CXX_HDRS := $(SRC_DIR)/bigint.hpp \
        $(SRC_DIR)/mod_arth.hpp \
        $(SRC_DIR)/ecc.hpp

V_CXXFLAGS := -std=c++23 -I$(abspath $(SRC_DIR)) -Wall
V_CXXFLAGS += -I$(abspath $(ROOT_DIR)/sim/utils/)
V_CXXFLAGS += -Wno-unknown-warning-option -Wno-unused-variable
V_LDFLAGS :=


ifeq ($(DEBUG), 1)
V_CXXFLAGS += -ggdb -fsanitize=address -fno-omit-frame-pointer
V_LDFLAGS += -fsanitize=address
TEST_ENV_VARS = ASAN_OPTIONS=symbolize=1 ASAN_SYMBOLIZER_PATH=$(shell which llvm-symbolizer 2>/dev/null)
$(info [INFO]: SIM Debug mode ON)
else
V_CXXFLAGS += -O3 -DNDEBUG
TEST_ENV_VARS =
$(info [INFO]: SIM Debug mode OFF)
endif


PARAM_NUMBER_SIZE := 256
PARAM_W := 64
PARAM_CHUNKS := $(shell echo $$(( $(PARAM_NUMBER_SIZE) / $(PARAM_W) )))
# PARAM_MODULUS := 73eda753299d7d483339d80809a1d80553bda402fffe5bfeffffffff00000001
PARAM_MODULUS := 30644e72e131a029b85045b68181585d97816a916871ca8d3c208c16d87cfd47

V_CXXFLAGS += -DHW_NUMBER_SIZE=$(PARAM_NUMBER_SIZE)
V_CXXFLAGS += -DHW_W=$(PARAM_W)
V_CXXFLAGS += -DHW_MODULUS='"$(PARAM_MODULUS)"'

V_FLAGS := --cc --exe --build -j $(shell nproc)
V_FLAGS += --trace-fst --trace-structs
V_FLAGS += --Mdir $(abspath $(BUILD_DIR))
V_FLAGS += --top-module $(TOP_MOD)
V_FLAGS += -CFLAGS "$(V_CXXFLAGS)" -LDFLAGS "$(V_LDFLAGS)"
V_FLAGS += --compiler clang
V_FLAGS += -MAKEFLAGS "CXX=$(CXX) LINK=$(CXX) -s"

V_FLAGS += -GNUMBER_SIZE=$(PARAM_NUMBER_SIZE)
V_FLAGS += -GW=$(PARAM_W)
V_FLAGS += "-GMODULUS=$(PARAM_NUMBER_SIZE)'h$(PARAM_MODULUS)"
V_FLAGS +=


.PHONY: all sim run compdb clean

all: sim

sim: $(BUILD_DIR)/V$(TOP_MOD)

$(BUILD_DIR)/V$(TOP_MOD): $(HDL_SRCS) $(TB_SRC) $(CXX_HDRS) Makefile
	@echo -e "\033[0;32m[VERILATOR] Building Simulation: V$(TOP_MOD)\033[0m"
	@mkdir -p $(BUILD_DIR)
	@CXX=$(CXX) $(VERILATOR) $(V_FLAGS) $(HDL_SRCS) $(TB_SRC)

run: sim
	@echo -e "\033[0;32m[SIM] Running $(TOP_MOD) testbench...\033[0m"
	@./$(BUILD_DIR)/V$(TOP_MOD)

compdb:
	bear --output compile_commands.json -- $(MAKE) sim --always-make

clean:
	@echo "[CLEAN] Removing $(BUILD_DIR)"
	@rm -rf $(BUILD_DIR)

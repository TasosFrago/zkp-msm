MAKEFLAGS += -j$(shell nproc)
MAKEFLAGS += --no-print-directory

CCACHE := $(shell command -v ccache 2> /dev/null)
CXX := clang++

DEBUG ?= 1
BUILD_DIR = build

# Compilers flags
FLAGS = -fuse-ld=mold -Wall -Werror -Wno-\#warnings -Wno-c++26-extensions

## ======================== COMPILER CHECK =====================================
ifneq (,$(findstring clang,$(CXX))) # Using CLANG LLVM
FLAGS += -Wno-unused-command-line-argument -fcolor-diagnostics -fansi-escape-codes
TEST_ENV_VARS = ASAN_OPTIONS=symbolize=1 ASAN_SYMBOLIZER_PATH=$(shell which llvm-symbolizer)
TARGET_NAME := _cl
$(info [INFO]: Detected Clang compiler)
else ifneq (,$(findstring g++,$(CXX))) # Using GCC G++
FLAGS += -fdiagnostics-color=always
TEST_ENV_VARS = ASAN_OPTIONS=symbolize=1
TARGET_NAME := _gc
$(info [INFO]: Detected GCC compiler)
else
$(error [ERROR]: Unknown compiler given)
endif
## =============================================================================

## =========================== DEBUG CHECK =====================================
ifeq ($(DEBUG), 1)
FLAGS += -ggdb -fsanitize=address -fno-omit-frame-pointer
TARGET_NAME := $(TARGET_NAME)_debug
$(info [INFO]: Debug mode ON)
else
FLAGS += -O3 -DNDEBUG
TARGET_NAME := $(TARGET_NAME)_release
$(info [INFO]: Debug mode OFF)
endif
## =============================================================================

INCLUDES = -I./src/ -I./cmd/ -I$(BUILD_DIR)/gen/
CXXFLAGS = -std=c++23 -MMD -MP $(FLAGS) $(INCLUDES)
LIBS = -ltbb

## === SOURCE CDOE ===
PRIMES_H := $(BUILD_DIR)/gen/primes.h

CXX_SRCS := $(shell find src -type f -name "*.cpp")
CXX_OBJS = $(patsubst %.cpp, $(BUILD_DIR)/%.o, $(CXX_SRCS))

CMD_NAMES := $(patsubst ./%,%,$(wildcard ./cmd/*.cpp))
CMD_BINS := $(patsubst cmd/%.cpp, $(BUILD_DIR)/%$(TARGET_NAME), $(CMD_NAMES))

ALL_SRCS := $(CXX_SRCS) $(CMD_NAMES)
DEPS = $(patsubst %.cpp, $(BUILD_DIR)/%.d, $(ALL_SRCS))



##---------------
## BUILD RULES
##---------------
.PHONY: all _build_all clean run-%

TIME_FMT := $(shell printf "\n\033[1;32mTotal Build Time: [ \033[1;36m%%es\033[1;32m ]\033[0m")

all:
	@echo -e "\033[0;32m========== Building all targets =========\033[0m"
	@/usr/bin/time -f "$(TIME_FMT)" $(MAKE) _build_all

run-%:
	@/usr/bin/time -f "$(TIME_FMT)" $(MAKE) $(BUILD_DIR)/$*$(TARGET_NAME)

	@echo
	@echo -e "\033[0;32m============ Running $* ============\033[0m"
	@$(TEST_ENV_VARS) ./$(BUILD_DIR)/$*$(TARGET_NAME) $(ARGS)

%: $(BUILD_DIR)/%$(TARGET_NAME)
	@:

_build_all: $(CMD_BINS) $(PRIMES_H)

$(PRIMES_H): $(BUILD_DIR)/primesGen$(TARGET_NAME)
	@mkdir -p $(dir $@)
	@echo "[GEN]: Creating $@"
	@./$(BUILD_DIR)/primesGen$(TARGET_NAME) -o $@ -n 20 -d 15

$(BUILD_DIR)/tests$(TARGET_NAME): $(PRIMES_H)

$(CMD_BINS): $(BUILD_DIR)/%$(TARGET_NAME): $(BUILD_DIR)/cmd/%.o $(CXX_OBJS)
	@mkdir -p $(dir $@)
	@echo "[LINKING]: $@"
	@$(CCACHE) $(CXX) $(CXXFLAGS) -o $@ $(filter %.o, $^) $(LIBS)

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	@echo "[BUILDING]: $@"
	@$(CCACHE) $(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	-rm -rf $(BUILD_DIR)

-include $(DEPS)

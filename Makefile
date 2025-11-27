MAKEFLAGS += -j$(shell nproc)

CXX = clang++

DEBUG ?= 1
BUILD_DIR = build

# Compilers flags
FLAGS = -Wall -Werror -Wno-\#warnings

ifneq (,$(findstring clang,$(CXX)))
FLAGS += -Wno-unused-command-line-argument -fcolor-diagnostics -fansi-escape-codes
TEST_ENV_VARS = ASAN_OPTIONS=symbolize=1 ASAN_SYMBOLIZER_PATH=$(shell which llvm-symbolizer)
$(info [INFO]: Detected Clang compiler)
else ifneq (,$(findstring g++,$(CXX)))
FLAGS += -fdiagnostics-color=always
TEST_ENV_VARS = ASAN_OPTIONS=symbolize=1
$(info [INFO]: Detected GCC compiler)
else
$(error [ERROR]: Unknown compiler given)
endif

## === DEBUG CHECK ===
ifeq ($(DEBUG), 1)
FLAGS += -ggdb -fsanitize=address -fno-omit-frame-pointer
$(info [INFO]: Debug mode ON)
else
FLAGS += -O3 -DNDEBUG
$(info [INFO]: Debug mode OFF)
endif
## ===================

INCLUDES = -I./src/ -I./cmd/
CXXFLAGS = -std=c++23 -MMD -MP $(FLAGS) $(INCLUDES)

## === SOURCE CDOE ===
CXX_SRCS = ./cmd/main.cpp \
	   ./cmd/bc_wrapper.cpp \
	   ./cmd/cmp_tests.cpp \
	   ./cmd/math_tests.cpp

TARGET = $(BUILD_DIR)/tests

CXX_OBJS = $(patsubst ./%.cpp, $(BUILD_DIR)/%.o, $(CXX_SRCS))
DEPS = $(CXX_OBJS:.o=.d)



##---------------
## BUILD RULES
##---------------
.PHONY: all test clean

all: $(TARGET)

$(TARGET): $(CXX_OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BUILD_DIR)/%.o: ./%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

test: $(TARGET)
	@echo
	@echo -e "\033[0;32m============ Running program ============\033[0m"
	@$(TEST_ENV_VARS) ./$(TARGET)

clean:
	-rm -rf $(BUILD_DIR)

-include $(DEPS)

# Concurrency test build/run
CONCURRENCY_TEST_SRC = integration_test/concurrency_test.c
CONCURRENCY_TEST_BIN = $(BUILD_DIR)/concurrency_test$(EXE_EXT)

# Compiler and flags
CC = gcc
BASE_CFLAGS = -Wall -Wextra -g
CFLAGS = $(BASE_CFLAGS)
COVERAGE_FLAGS = --coverage -fprofile-arcs -ftest-coverage

ifeq ($(OS),Windows_NT)
EXE_EXT = .exe
RUN_PREFIX = .\\
define MKDIR_P
powershell -NoProfile -Command "New-Item -ItemType Directory -Force -Path '$(1)' | Out-Null"
endef
define RM_RF
powershell -NoProfile -Command "if (Test-Path '$(1)') { Remove-Item -Recurse -Force '$(1)' }"
endef
define CLEAN_FILES
powershell -NoProfile -Command "Remove-Item -Force -ErrorAction SilentlyContinue *.gcov,*.gcda,*.gcno,coverage.info; exit 0"
endef
else
CFLAGS += -fno-pie -no-pie -Wa,--noexecstack
EXE_EXT =
RUN_PREFIX = ./
define MKDIR_P
mkdir -p "$(1)"
endef
define RM_RF
rm -rf "$(1)"
endef
define CLEAN_FILES
rm -f *.gcov *.gcda *.gcno coverage.info
endef
endif

# Common build macro
BUILD_CMD = $(CC) $(CFLAGS) $(INCLUDES) $(EXTRA_FLAGS)

# Directories
UNITY_DIR     = ./include
KEYSTORE_DIR  = ../../src/keystore
TEST_DIR      = ./unit_tests
BUILD_DIR     = bin
COVERAGE_DIR  = coverage

# Include paths
INCLUDES = -I$(UNITY_DIR) -I$(KEYSTORE_DIR)

# Source files
UNITY_SRC = $(UNITY_DIR)/unity.c
TEST_SRC  = $(TEST_DIR)/test_runner.c

# List of .c files to exclude from build
KEYSTORE_EXCLUDE = bucket/%.c

# Collect keystore .c files (up to 5 nested levels), then filter exclusions
KEYSTORE_SRC := $(filter-out $(addprefix $(KEYSTORE_DIR)/,$(KEYSTORE_EXCLUDE)), \
	$(wildcard $(KEYSTORE_DIR)/*.c) \
	$(wildcard $(KEYSTORE_DIR)/*/*.c) \
	$(wildcard $(KEYSTORE_DIR)/*/*/*.c) \
	$(wildcard $(KEYSTORE_DIR)/*/*/*/*.c) \
	$(wildcard $(KEYSTORE_DIR)/*/*/*/*/*.c) \
	$(wildcard $(KEYSTORE_DIR)/*/*/*/*/*/*.c))

# Map source files to object files in build directory
KEYSTORE_OBJS := $(patsubst $(KEYSTORE_DIR)/%.c,$(BUILD_DIR)/%.o,$(KEYSTORE_SRC))

# Unity and test object files
UNITY_OBJ = $(BUILD_DIR)/unity.o
TEST_OBJ  = $(BUILD_DIR)/test_runner.o

# Test executable
TEST_BIN = $(BUILD_DIR)/key_store_test$(EXE_EXT)

# Default target
all: test

# Create build directory and subdirectories
$(BUILD_DIR):
	@$(call RM_RF,$(BUILD_DIR))
	@$(call MKDIR_P,$(BUILD_DIR))

# Create coverage directory
coverage-dir:
	@$(call MKDIR_P,$(COVERAGE_DIR))

# Compile Unity framework
$(UNITY_OBJ): $(UNITY_SRC) | $(BUILD_DIR)
	$(BUILD_CMD) -c $< -o $@

# Compile test runner
$(TEST_OBJ): $(TEST_SRC) | $(BUILD_DIR)
	$(BUILD_CMD) -c $< -o $@

# Compile keystore sources (unit test build)
$(BUILD_DIR)/%.o: $(KEYSTORE_DIR)/%.c | $(BUILD_DIR)
	@$(call MKDIR_P,$(dir $@))
	$(CC) $(CFLAGS) $(INCLUDES) $(UNIT_FLAGS) -c $< -o $@

# Link unit test executable **FIXED: -lpthread -lm**
$(TEST_BIN): $(UNITY_OBJ) $(TEST_OBJ) $(KEYSTORE_OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $(UNIT_FLAGS) $^ -lpthread -lm -o $@

# Build unit test (with coverage)
unit_build:
	$(MAKE) UNIT_FLAGS="$(COVERAGE_FLAGS)" $(TEST_BIN)

# Run unit tests
test: unit_build
	@echo "Running unit tests..."
	$(RUN_PREFIX)$(TEST_BIN)

# Build test binary for Valgrind (no coverage/sanitizer)
valgrind_build:
	$(MAKE) UNIT_FLAGS="" $(TEST_BIN)

# Run unit tests under Valgrind
valgrind-test: valgrind_build
	@echo "Running unit tests under Valgrind..."
	valgrind --leak-check=full --track-origins=yes ./$(TEST_BIN)

# Full coverage report (requires lcov)
coverage: test | coverage-dir
	@echo "Generating code coverage report..."
	$(foreach src,$(KEYSTORE_SRC), \
	  gcov -o $(dir $(patsubst $(KEYSTORE_DIR)/%.c,$(BUILD_DIR)/%.o,$(src))) $(src);)
ifeq ($(LCOV),1)
	lcov --capture --directory $(BUILD_DIR) --output-file $(COVERAGE_DIR)/coverage.info
	lcov --remove $(COVERAGE_DIR)/coverage.info '/usr/*' --output-file $(COVERAGE_DIR)/coverage.info
	lcov --remove $(COVERAGE_DIR)/coverage.info '*test*' --output-file $(COVERAGE_DIR)/coverage.info
	lcov --remove $(COVERAGE_DIR)/coverage.info '*unity*' --output-file $(COVERAGE_DIR)/coverage.info
	genhtml $(COVERAGE_DIR)/coverage.info --output-directory $(COVERAGE_DIR)/html
	@echo "HTML report in $(COVERAGE_DIR)/html/index.html"
else
	@echo "Skipping HTML coverage. Set LCOV=1 to enable."
endif

# Quick coverage without HTML
coverage-simple: test
	@echo "Generating simple coverage report..."
	$(foreach src,$(KEYSTORE_SRC), \
	  gcov -o $(dir $(patsubst $(KEYSTORE_DIR)/%.c,$(BUILD_DIR)/%.o,$(src))) $(src);)
	@echo "Simple coverage files in $(BUILD_DIR)/*/*.gcov"

# Build concurrency test (no coverage)
concurrency_build:
	$(MAKE) clean
	$(MAKE) UNIT_FLAGS="" EXTRA_FLAGS="" $(CONCURRENCY_TEST_BIN)

# Link concurrency test **FIXED: -lpthread -lm**
$(CONCURRENCY_TEST_BIN): $(CONCURRENCY_TEST_SRC) $(KEYSTORE_OBJS) | $(BUILD_DIR)
	$(BUILD_CMD) $(CONCURRENCY_TEST_SRC) $(KEYSTORE_OBJS) -lpthread -lm -o $@

run-concurrency-test: concurrency_build
	@echo "Running concurrency test..."
	$(RUN_PREFIX)$(CONCURRENCY_TEST_BIN)

run-ct-valgrind: concurrency_build
	@echo "Running concurrency test under Valgrind..."
	valgrind --leak-check=full --track-origins=yes $(RUN_PREFIX)$(CONCURRENCY_TEST_BIN)

# Debug detected dirs/files
debug:
	@echo "Source files:    $(KEYSTORE_SRC)"
	@echo "Object files:    $(KEYSTORE_OBJS)"

# Clean build and coverage artifacts
clean:
	@$(call RM_RF,$(BUILD_DIR))
	@$(call RM_RF,$(COVERAGE_DIR))
	@$(call CLEAN_FILES)

# Phony targets
.PHONY: all test clean coverage coverage-simple coverage-dir debug run-concurrency-test run-ct-valgrind concurrency_build help

# Help message
help:
	@echo "Available targets:"
	@echo "  all                 - Build and run tests"
	@echo "  test                - Build and run unit tests"
	@echo "  coverage            - Full coverage (LCOV=1 for HTML)"
	@echo "  coverage-simple     - Quick .gcov files"
	@echo "  run-concurrency-test - Build/run concurrency test"
	@echo "  run-ct-valgrind     - Run concurrency test under Valgrind"
	@echo "  debug               - Show detected files"
	@echo "  clean               - Clean artifacts"
	@echo "  help                - This message"

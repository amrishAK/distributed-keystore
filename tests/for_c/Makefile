# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -g
COVERAGE_FLAGS = --coverage -fprofile-arcs -ftest-coverage

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

# Auto-detect immediate subdirectories inside keystore
KEYSTORE_SUBDIRS := $(shell ls -d $(KEYSTORE_DIR)/*/ 2>/dev/null | xargs -n1 basename)

# Collect all .c files from detected subdirectories
KEYSTORE_SRC := $(foreach d,$(KEYSTORE_SUBDIRS),$(wildcard $(KEYSTORE_DIR)/$(d)/*.c))

# Map source files to object files in build directory
KEYSTORE_OBJS := $(patsubst $(KEYSTORE_DIR)/%.c,$(BUILD_DIR)/%.o,$(KEYSTORE_SRC))

# Unity and test object files
UNITY_OBJ = $(BUILD_DIR)/unity.o
TEST_OBJ  = $(BUILD_DIR)/test_runner.o

# Test executable
TEST_BIN = $(BUILD_DIR)/key_store_test

# Default target
all: test

# Create build directory and its subdirectories
$(BUILD_DIR):
	touch $(BUILD_DIR) # ensure the directory exists
	rm -rf $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)
	$(foreach d,$(KEYSTORE_SUBDIRS), mkdir -p $(BUILD_DIR)/$(d);)

# Create coverage directory
coverage-dir:
	mkdir -p $(COVERAGE_DIR)

# Compile Unity framework
$(UNITY_OBJ): $(UNITY_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Compile test files
$(TEST_OBJ): $(TEST_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Compile keystore sources
$(BUILD_DIR)/%.o: $(KEYSTORE_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(COVERAGE_FLAGS) $(INCLUDES) -c $< -o $@

# Link executable
$(TEST_BIN): $(UNITY_OBJ) $(TEST_OBJ) $(KEYSTORE_OBJS)
	$(CC) $(CFLAGS) $(COVERAGE_FLAGS) $^ -o $@

# Run unit tests
test: $(TEST_BIN)
	@echo "Running unit tests..."
	./$(TEST_BIN)

# Full coverage report (requires lcov)
coverage: test | coverage-dir
	@echo "Generating code coverage report..."
	$(foreach d,$(KEYSTORE_SUBDIRS), \
	  gcov -o $(BUILD_DIR)/$(d) $(wildcard $(KEYSTORE_DIR)/$(d)/*.c);)
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
	$(foreach d,$(KEYSTORE_SUBDIRS), \
	  gcov -o $(BUILD_DIR)/$(d) $(wildcard $(KEYSTORE_DIR)/$(d)/*.c);)
	@echo "Simple coverage files in $(BUILD_DIR)/*/*.gcov"

# Debug detected dirs and files
debug:
	@echo "Subdirectories: $(KEYSTORE_SUBDIRS)"
	@echo "Source files:    $(KEYSTORE_SRC)"
	@echo "Object files:    $(KEYSTORE_OBJS)"

# Clean build and coverage artifacts
clean:
	rm -rf $(BUILD_DIR) $(COVERAGE_DIR) *.gcov *.gcda *.gcno coverage.info

# Phony targets
.PHONY: all test clean coverage coverage-simple coverage-dir debug help

# Help message
help:
	@echo "Available targets:"
	@echo "  all             - Build and run tests"
	@echo "  test            - Build and run tests"
	@echo "  coverage        - Full coverage report (LCOV=1 for HTML)"
	@echo "  coverage-simple - Quick .gcov coverage report"
	@echo "  debug           - Show detected dirs and files"
	@echo "  clean           - Remove build/coverage artifacts"
	@echo "  help            - Show this help message"

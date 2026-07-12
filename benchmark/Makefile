# Compiler and flags
CC = gcc
PYTHON ?= python3
CFLAGS = -std=c11 -O2 -Wall -Wextra -pedantic -D_POSIX_C_SOURCE=200809L
LDFLAGS = -lpthread -lm

ifeq ($(OS),Windows_NT)
define MKDIR_P
powershell -NoProfile -Command "New-Item -ItemType Directory -Force -Path '$(1)' | Out-Null"
endef
define RM_RF
powershell -NoProfile -Command "if (Test-Path '$(1)') { Remove-Item -Recurse -Force '$(1)' }"
endef
RUN_PREFIX = .\\
EXE_EXT = .exe
LDFLAGS += -lpsapi
else
define MKDIR_P
mkdir -p "$(1)"
endef
define RM_RF
rm -rf "$(1)"
endef
RUN_PREFIX = ./
EXE_EXT =
endif

# Common build macro
BUILD_CMD = $(CC) $(CFLAGS) $(INCLUDES) $(EXTRA_FLAGS)

# Directories
ROOT_DIR      = ..
KEYSTORE_DIR  = $(ROOT_DIR)/src/keystore
BENCH_SRC_DIR = src
COMMON_DIR    = $(BENCH_SRC_DIR)/common
ST_DIR        = $(BENCH_SRC_DIR)/single_thread
MT_DIR        = $(BENCH_SRC_DIR)/multi_thread
BUILD_DIR     = bin
BUILD_PREP    = build-dir
RESULTS_DIR   = .result

# Include paths
INCLUDES = -I$(KEYSTORE_DIR) -I$(COMMON_DIR) -I$(ST_DIR) -I$(MT_DIR)

# Benchmark source files
BENCH_COMMON_SRC = $(COMMON_DIR)/benchmark_common.c
ST_MAIN_SRC      = $(ST_DIR)/single_thread_benchmark.c
ST_MODELS_SRC    = $(ST_DIR)/single_thread_models.c
ST_RUNNER_SRC    = $(ST_DIR)/single_thread_runner.c
MT_MAIN_SRC      = $(MT_DIR)/multi_thread_benchmark.c
MT_MODELS_SRC    = $(MT_DIR)/multi_thread_models.c
MT_RUNNER_SRC    = $(MT_DIR)/multi_thread_runner.c

# Collect keystore .c files (up to 5 nested levels)
KEYSTORE_SRC := $(wildcard $(KEYSTORE_DIR)/*.c) \
	$(wildcard $(KEYSTORE_DIR)/*/*.c) \
	$(wildcard $(KEYSTORE_DIR)/*/*/*.c) \
	$(wildcard $(KEYSTORE_DIR)/*/*/*/*.c) \
	$(wildcard $(KEYSTORE_DIR)/*/*/*/*/*.c) \
	$(wildcard $(KEYSTORE_DIR)/*/*/*/*/*/*.c)

# Map source files to object files in build directory
KEYSTORE_OBJS     := $(patsubst $(KEYSTORE_DIR)/%.c,$(BUILD_DIR)/%.o,$(KEYSTORE_SRC))
BENCH_COMMON_OBJ  = $(BUILD_DIR)/$(COMMON_DIR)/benchmark_common.o
ST_MAIN_OBJ       = $(BUILD_DIR)/$(ST_DIR)/single_thread_benchmark.o
ST_MODELS_OBJ     = $(BUILD_DIR)/$(ST_DIR)/single_thread_models.o
ST_RUNNER_OBJ     = $(BUILD_DIR)/$(ST_DIR)/single_thread_runner.o
MT_MAIN_OBJ       = $(BUILD_DIR)/$(MT_DIR)/multi_thread_benchmark.o
MT_MODELS_OBJ     = $(BUILD_DIR)/$(MT_DIR)/multi_thread_models.o
MT_RUNNER_OBJ     = $(BUILD_DIR)/$(MT_DIR)/multi_thread_runner.o
SINGLE_THREAD_OBJS = $(ST_MAIN_OBJ) $(ST_MODELS_OBJ) $(ST_RUNNER_OBJ) $(BENCH_COMMON_OBJ)
MULTI_THREAD_OBJS  = $(MT_MAIN_OBJ) $(MT_MODELS_OBJ) $(MT_RUNNER_OBJ) $(BENCH_COMMON_OBJ)

# Output binaries and reports
BENCH_BIN       = $(BUILD_DIR)/single_thread_benchmark$(EXE_EXT)
MT_BIN          = $(BUILD_DIR)/multi_thread_benchmark$(EXE_EXT)
RESULTS_FILE    = $(RESULTS_DIR)/single_thread_results.jsonl
MT_RESULTS_FILE = $(RESULTS_DIR)/multi_thread_results.jsonl
MT_HIGH_RESULTS_FILE = $(RESULTS_DIR)/multi_thread_high_scale_results.jsonl
MT_AB_RESULTS_FILE = $(RESULTS_DIR)/multi_thread_ab_results.jsonl
MT_AB_TEST_RESULTS_FILE = $(RESULTS_DIR)/multi_thread_ab_test_results.jsonl
MT_CORE_RESULTS_FILE = $(RESULTS_DIR)/multi_thread_core_results.jsonl
MT_DIST_RESULTS_FILE = $(RESULTS_DIR)/multi_thread_distribution_results.jsonl
MT_RESIZE_RESULTS_FILE = $(RESULTS_DIR)/multi_thread_resize_results.jsonl
MT_VKS_RESULTS_FILE = $(RESULTS_DIR)/multi_thread_value_keyspace_results.jsonl
REPORT_FILE     = $(RESULTS_DIR)/single_thread_report.html
MT_REPORT_FILE  = $(RESULTS_DIR)/multi_thread_report.html
MT_HIGH_REPORT_FILE = $(RESULTS_DIR)/multi_thread_high_scale_report.html
MT_AB_REPORT_FILE = $(RESULTS_DIR)/multi_thread_ab_report.html
MT_AB_TEST_REPORT_FILE = $(RESULTS_DIR)/multi_thread_ab_test_report.html
MT_CORE_REPORT_FILE = $(RESULTS_DIR)/multi_thread_core_report.html
MT_DIST_REPORT_FILE = $(RESULTS_DIR)/multi_thread_distribution_report.html
MT_RESIZE_REPORT_FILE = $(RESULTS_DIR)/multi_thread_resize_report.html
MT_VKS_REPORT_FILE = $(RESULTS_DIR)/multi_thread_value_keyspace_report.html
MT_HIGH_SCENARIO = MT2-OVER-003
MT_CORE_SCENARIO_PREFIX = MT2-CORE-
MT_MIX_SCENARIO_PREFIX = MT2-MIX-
MT_DIST_SCENARIO_PREFIX = MT2-DIST-
MT_RESIZE_SCENARIO_PREFIX = MT2-RSZ-
MT_VKS_SCENARIO_PREFIX = MT2-VKS-
MT_AB_SCENARIO_PREFIX = MT2-MIX-
MT_AB_TEST_SCENARIO_PREFIX = MT2-VKS-

# Default target
all: benchmark

# Prepare build directory
$(BUILD_PREP):
	@$(call MKDIR_P,$(BUILD_DIR))

# Create results directory
results-dir:
	@$(call MKDIR_P,$(RESULTS_DIR))

# Compile benchmark sources
$(BENCH_COMMON_OBJ): $(BENCH_COMMON_SRC) | $(BUILD_PREP)
	@$(call MKDIR_P,$(dir $@))
	$(BUILD_CMD) -c $< -o $@

$(ST_MAIN_OBJ): $(ST_MAIN_SRC) | $(BUILD_PREP)
	@$(call MKDIR_P,$(dir $@))
	$(BUILD_CMD) -c $< -o $@

$(ST_MODELS_OBJ): $(ST_MODELS_SRC) | $(BUILD_PREP)
	@$(call MKDIR_P,$(dir $@))
	$(BUILD_CMD) -c $< -o $@

$(ST_RUNNER_OBJ): $(ST_RUNNER_SRC) | $(BUILD_PREP)
	@$(call MKDIR_P,$(dir $@))
	$(BUILD_CMD) -c $< -o $@

$(MT_MAIN_OBJ): $(MT_MAIN_SRC) | $(BUILD_PREP)
	@$(call MKDIR_P,$(dir $@))
	$(BUILD_CMD) -c $< -o $@

$(MT_MODELS_OBJ): $(MT_MODELS_SRC) | $(BUILD_PREP)
	@$(call MKDIR_P,$(dir $@))
	$(BUILD_CMD) -c $< -o $@

$(MT_RUNNER_OBJ): $(MT_RUNNER_SRC) | $(BUILD_PREP)
	@$(call MKDIR_P,$(dir $@))
	$(BUILD_CMD) -c $< -o $@

# Compile keystore sources
$(BUILD_DIR)/%.o: $(KEYSTORE_DIR)/%.c | $(BUILD_PREP)
	@$(call MKDIR_P,$(dir $@))
	$(BUILD_CMD) -c $< -o $@

# Link benchmark binaries
$(BENCH_BIN): $(SINGLE_THREAD_OBJS) $(KEYSTORE_OBJS) | $(BUILD_PREP)
	$(CC) $(CFLAGS) $(INCLUDES) $^ $(LDFLAGS) -o $@

$(MT_BIN): $(MULTI_THREAD_OBJS) $(KEYSTORE_OBJS) | $(BUILD_PREP)
	$(CC) $(CFLAGS) $(INCLUDES) $^ $(LDFLAGS) -o $@

# High-level targets
bin: $(BENCH_BIN)

multi-build: $(MT_BIN)

run: bin | results-dir
	$(RUN_PREFIX)$(BENCH_BIN) --results-file $(RESULTS_FILE)

multi-run: multi-build | results-dir
	$(RUN_PREFIX)$(MT_BIN) --results-file $(MT_RESULTS_FILE)

multi-run-core: multi-build | results-dir
	$(RUN_PREFIX)$(MT_BIN) --scenario-prefix $(MT_CORE_SCENARIO_PREFIX) --results-file $(MT_CORE_RESULTS_FILE)

multi-run-high: multi-build | results-dir
	$(RUN_PREFIX)$(MT_BIN) --scenario $(MT_HIGH_SCENARIO) --results-file $(MT_HIGH_RESULTS_FILE)

multi-run-ab: multi-build | results-dir
	$(RUN_PREFIX)$(MT_BIN) --scenario-prefix $(MT_AB_SCENARIO_PREFIX) --results-file $(MT_AB_RESULTS_FILE)

multi-run-ab-test: multi-build | results-dir
	$(RUN_PREFIX)$(MT_BIN) --scenario-prefix $(MT_AB_TEST_SCENARIO_PREFIX) --results-file $(MT_AB_TEST_RESULTS_FILE)

multi-run-dist: multi-build | results-dir
	$(RUN_PREFIX)$(MT_BIN) --scenario-prefix $(MT_DIST_SCENARIO_PREFIX) --results-file $(MT_DIST_RESULTS_FILE)

multi-run-resize: multi-build | results-dir
	$(RUN_PREFIX)$(MT_BIN) --scenario-prefix $(MT_RESIZE_SCENARIO_PREFIX) --results-file $(MT_RESIZE_RESULTS_FILE)

multi-run-vks: multi-build | results-dir
	$(RUN_PREFIX)$(MT_BIN) --scenario-prefix $(MT_VKS_SCENARIO_PREFIX) --results-file $(MT_VKS_RESULTS_FILE)

report: run
	@where $(PYTHON) >NUL 2>NUL && $(PYTHON) scripts/report.py --input $(RESULTS_FILE) --output $(REPORT_FILE) || echo Python executable '$(PYTHON)' not found. Skipping report generation.

multi-report: multi-run
	@where $(PYTHON) >NUL 2>NUL && $(PYTHON) scripts/report.py --input $(MT_RESULTS_FILE) --output $(MT_REPORT_FILE) || echo Python executable '$(PYTHON)' not found. Skipping report generation.

multi-report-core: multi-run-core
	@where $(PYTHON) >NUL 2>NUL && $(PYTHON) scripts/report.py --input $(MT_CORE_RESULTS_FILE) --output $(MT_CORE_REPORT_FILE) || echo Python executable '$(PYTHON)' not found. Skipping report generation.

multi-report-high: multi-run-high
	@where $(PYTHON) >NUL 2>NUL && $(PYTHON) scripts/report.py --input $(MT_HIGH_RESULTS_FILE) --output $(MT_HIGH_REPORT_FILE) || echo Python executable '$(PYTHON)' not found. Skipping report generation.

multi-report-ab: multi-run-ab
	@where $(PYTHON) >NUL 2>NUL && $(PYTHON) scripts/report.py --input $(MT_AB_RESULTS_FILE) --output $(MT_AB_REPORT_FILE) || echo Python executable '$(PYTHON)' not found. Skipping report generation.

multi-report-ab-test: multi-run-ab-test
	@where $(PYTHON) >NUL 2>NUL && $(PYTHON) scripts/report.py --input $(MT_AB_TEST_RESULTS_FILE) --output $(MT_AB_TEST_REPORT_FILE) || echo Python executable '$(PYTHON)' not found. Skipping report generation.

multi-report-dist: multi-run-dist
	@where $(PYTHON) >NUL 2>NUL && $(PYTHON) scripts/report.py --input $(MT_DIST_RESULTS_FILE) --output $(MT_DIST_REPORT_FILE) || echo Python executable '$(PYTHON)' not found. Skipping report generation.

multi-report-resize: multi-run-resize
	@where $(PYTHON) >NUL 2>NUL && $(PYTHON) scripts/report.py --input $(MT_RESIZE_RESULTS_FILE) --output $(MT_RESIZE_REPORT_FILE) || echo Python executable '$(PYTHON)' not found. Skipping report generation.

multi-report-vks: multi-run-vks
	@where $(PYTHON) >NUL 2>NUL && $(PYTHON) scripts/report.py --input $(MT_VKS_RESULTS_FILE) --output $(MT_VKS_REPORT_FILE) || echo Python executable '$(PYTHON)' not found. Skipping report generation.

benchmark: run report

benchmark-multi: multi-run multi-report

benchmark-multi-core: multi-run-core multi-report-core

benchmark-multi-high: multi-run-high multi-report-high

benchmark-multi-ab: multi-run-ab multi-report-ab

benchmark-multi-ab-test: multi-run-ab-test multi-report-ab-test

benchmark-multi-dist: multi-run-dist multi-report-dist

benchmark-multi-resize: multi-run-resize multi-report-resize

benchmark-multi-vks: multi-run-vks multi-report-vks

benchmark-all: benchmark benchmark-multi

# Debug detected dirs/files
debug:
	@echo "Source files:    $(KEYSTORE_SRC)"
	@echo "Object files:    $(KEYSTORE_OBJS)"

# Clean artifacts
clean:
	@$(call RM_RF,$(BUILD_DIR))
	@$(call RM_RF,$(RESULTS_DIR))

# Phony targets
.PHONY: all bin run report benchmark multi-build multi-run multi-run-core multi-run-high multi-run-ab multi-run-ab-test multi-run-dist multi-run-resize multi-run-vks multi-report multi-report-core multi-report-high multi-report-ab multi-report-ab-test multi-report-dist multi-report-resize multi-report-vks benchmark-multi benchmark-multi-core benchmark-multi-high benchmark-multi-ab benchmark-multi-ab-test benchmark-multi-dist benchmark-multi-resize benchmark-multi-vks benchmark-all clean help debug results-dir build-dir

# Help message
help:
	@echo "Available targets:"
	@echo "  all             - Run single-thread benchmark and report"
	@echo "  bin             - Compile single-thread benchmark binary"
	@echo "  run             - Execute single-thread benchmark scenarios"
	@echo "  report          - Generate single-thread HTML report"
	@echo "  benchmark       - Run + report (single-thread)"
	@echo "  multi-build     - Compile multi-thread benchmark binary"
	@echo "  multi-run       - Execute multi-thread benchmark scenarios"
	@echo "  multi-run-core  - Execute core scaling scenarios"
	@echo "  multi-run-high  - Execute dedicated oversubscription stress scenario"
	@echo "  multi-run-ab    - Compatibility alias for workload-mix scenarios"
	@echo "  multi-run-ab-test - Compatibility alias for value/keyspace matrix scenarios"
	@echo "  multi-run-dist  - Execute distribution sensitivity scenarios"
	@echo "  multi-run-resize - Execute resize stress scenarios"
	@echo "  multi-run-vks   - Execute value-size/keyspace scenarios"
	@echo "  multi-report    - Generate multi-thread HTML report"
	@echo "  multi-report-core - Generate core scaling HTML report"
	@echo "  multi-report-high - Generate high-scale multi-thread HTML report"
	@echo "  multi-report-ab - Generate AB workload HTML report"
	@echo "  multi-report-ab-test - Generate Ab_Test scale matrix HTML report"
	@echo "  multi-report-dist - Generate distribution sensitivity HTML report"
	@echo "  multi-report-resize - Generate resize stress HTML report"
	@echo "  multi-report-vks - Generate value-size/keyspace HTML report"
	@echo "  benchmark-multi - Run + report (multi-thread)"
	@echo "  benchmark-multi-core - Run + report core scaling scenarios"
	@echo "  benchmark-multi-high - Run + report (high-scale multi-thread)"
	@echo "  benchmark-multi-ab - Run + report (AB multi-thread workloads)"
	@echo "  benchmark-multi-ab-test - Run + report (Ab_Test scale matrix)"
	@echo "  benchmark-multi-dist - Run + report distribution sensitivity scenarios"
	@echo "  benchmark-multi-resize - Run + report resize stress scenarios"
	@echo "  benchmark-multi-vks - Run + report value-size/keyspace scenarios"
	@echo "  benchmark-all   - Run and report both suites"
	@echo "  debug           - Show detected sources and objects"
	@echo "  clean           - Remove build and results artifacts"
	@echo "  help            - This message"

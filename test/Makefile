# ESP32 Rover Unit Tests Makefile
# These tests run on the host machine (not on ESP32)

CC = gcc
CFLAGS = -Wall -Wextra -g -I. -I../firmware/main
LDFLAGS =

# Coverage flags (used with coverage targets)
COVERAGE_CFLAGS = -fprofile-arcs -ftest-coverage
COVERAGE_LDFLAGS = --coverage

# Source files (in src/ subdirectory)
SRCS = src/test_runner.c src/test_config.c src/test_diag_state_machine.c
OBJS = $(SRCS:.c=.o)
TARGET = test_runner

# Coverage output directories
COVERAGE_DIR = coverage_report
COVERAGE_INFO = coverage.lcov

# Default target
.PHONY: all
all: $(TARGET)

# Build test runner
$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

# Compile source files
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Run tests
.PHONY: test
test: $(TARGET)
	./$(TARGET)

# Clean build artifacts
.PHONY: clean
clean:
	rm -f $(OBJS) $(TARGET)
	rm -f src/*.gcno src/*.gcda src/*.gcov
	rm -f *.gcno *.gcda *.gcov
	rm -f $(COVERAGE_INFO)
	rm -rf $(COVERAGE_DIR)

# Rebuild
.PHONY: rebuild
rebuild: clean all

# Dependencies
src/test_runner.o: src/test_runner.c
src/test_config.o: src/test_config.c unity.h ../firmware/main/config_generated.h
src/test_diag_state_machine.o: src/test_diag_state_machine.c unity.h

# ============================================================================
# Code Coverage Targets
# ============================================================================

# Build with coverage instrumentation
.PHONY: coverage-build
coverage-build: CFLAGS += $(COVERAGE_CFLAGS)
coverage-build: LDFLAGS += $(COVERAGE_LDFLAGS)
coverage-build: clean $(TARGET)

# Run tests and generate coverage data
.PHONY: coverage
coverage: coverage-build
	@echo "Running tests with coverage instrumentation..."
	./$(TARGET)
	@echo ""
	@echo "Generating coverage summary..."
	gcov -r src/*.c 2>/dev/null || true
	@echo ""
	@echo "Coverage data generated. Run 'make coverage-html' for detailed report."

# Generate HTML coverage report (requires lcov)
.PHONY: coverage-html
coverage-html: coverage
	@echo "Generating HTML coverage report..."
	@if command -v lcov > /dev/null 2>&1; then \
		lcov --capture --directory . --output-file $(COVERAGE_INFO) --ignore-errors source 2>/dev/null; \
		lcov --remove $(COVERAGE_INFO) '/usr/*' --output-file $(COVERAGE_INFO) 2>/dev/null; \
		genhtml $(COVERAGE_INFO) --output-directory $(COVERAGE_DIR) 2>/dev/null; \
		echo ""; \
		echo "HTML coverage report: $(COVERAGE_DIR)/index.html"; \
	else \
		echo "lcov not installed. Install with:"; \
		echo "  brew install lcov (macOS)"; \
		echo "  apt install lcov (Linux)"; \
	fi

# CI-friendly coverage check (fails if coverage below threshold)
.PHONY: coverage-check
coverage-check: coverage
	@echo "Checking coverage threshold..."
	@# This is a placeholder - actual threshold checking would require parsing gcov output

.PHONY: help
help:
	@echo "ESP32 Rover Unit Tests"
	@echo ""
	@echo "Targets:"
	@echo "  all           - Build test runner (default)"
	@echo "  test          - Build and run tests"
	@echo "  clean         - Remove build artifacts and coverage data"
	@echo "  rebuild       - Clean and rebuild"
	@echo ""
	@echo "Coverage targets:"
	@echo "  coverage      - Run tests with coverage, show summary"
	@echo "  coverage-html - Generate HTML coverage report (requires lcov)"
	@echo "  coverage-check - Check coverage meets threshold (CI use)"
	@echo ""
	@echo "  help          - Show this help"

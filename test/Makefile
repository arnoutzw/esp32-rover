# ESP32 Rover Unit Tests Makefile
# These tests run on the host machine (not on ESP32)

CC = gcc
CFLAGS = -Wall -Wextra -g -I. -I../firmware/main
LDFLAGS =

# Source files (in src/ subdirectory)
SRCS = src/test_runner.c src/test_config.c src/test_diag_state_machine.c
OBJS = $(SRCS:.c=.o)
TARGET = test_runner

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

# Rebuild
.PHONY: rebuild
rebuild: clean all

# Dependencies
src/test_runner.o: src/test_runner.c
src/test_config.o: src/test_config.c unity.h ../firmware/main/config_generated.h
src/test_diag_state_machine.o: src/test_diag_state_machine.c unity.h

.PHONY: help
help:
	@echo "ESP32 Rover Unit Tests"
	@echo ""
	@echo "Targets:"
	@echo "  all      - Build test runner (default)"
	@echo "  test     - Build and run tests"
	@echo "  clean    - Remove build artifacts"
	@echo "  rebuild  - Clean and rebuild"
	@echo "  help     - Show this help"

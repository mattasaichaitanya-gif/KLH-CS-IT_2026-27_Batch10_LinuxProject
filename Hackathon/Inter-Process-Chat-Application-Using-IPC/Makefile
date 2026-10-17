# ==============================================================================
# Makefile: Inter-Process Chat Application Using IPC (POSIX Named Pipes)
# Compiler: GCC
# Standards: C11 / POSIX.1-2008
# ==============================================================================

CC      := gcc
CFLAGS  := -Wall -Wextra -Wpedantic -std=c11 -O2
LDFLAGS := 

TARGET     := ipc-chat
BIN_DIR    := bin
SRC_DIR    := src
TEST_DIR   := tests
TARGET_BIN := $(BIN_DIR)/$(TARGET)

SRC        := chat.c
TEST_SRC   := $(TEST_DIR)/test_runner.c
TEST_BIN   := $(BIN_DIR)/test_runner

.PHONY: all clean run-alice run-bob test help

all: $(TARGET) $(TARGET_BIN) $(TEST_BIN)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(TARGET_BIN): $(TARGET)
	@mkdir -p $(BIN_DIR)
	cp $(TARGET) $(TARGET_BIN)

test: $(TEST_BIN)
	@echo "=========================================================="
	@echo " Running Automated IPC Chat Integration Test Suite"
	@echo "=========================================================="
	./$(TEST_BIN)

$(TEST_BIN): $(TEST_SRC) $(TARGET)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(TEST_SRC) $(LDFLAGS)

clean:
	@echo "Cleaning build artifacts and temporary FIFO endpoints..."
	rm -f $(TARGET) $(TARGET_BIN) $(TEST_BIN)
	rm -rf $(BIN_DIR)
	rm -f /tmp/ipc_chat_* /tmp/chat_fifo_*

help:
	@echo "Available targets:"
	@echo "  make         - Compile $(TARGET) with -Wall -Wextra -Wpedantic"
	@echo "  make test    - Build and run automated integration test runner"
	@echo "  make clean   - Remove binaries and /tmp/ipc_chat_* FIFO files"
	@echo "  make help    - Show this help summary"

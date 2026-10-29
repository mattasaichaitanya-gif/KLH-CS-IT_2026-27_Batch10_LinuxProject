CC = gcc
CFLAGS = -Wall -Wextra -pthread -O2 -Iinclude -D_POSIX_C_SOURCE=200809L
DEBUG_FLAGS = -Wall -Wextra -pthread -g -O0 -Iinclude -D_POSIX_C_SOURCE=200809L
LDFLAGS = -pthread

SRC_DIR = src
INC_DIR = include
TEST_DIR = tests
BIN_DIR = bin

SRCS = $(SRC_DIR)/buffer.c $(SRC_DIR)/sync.c $(SRC_DIR)/producer_consumer.c
OBJS = $(SRCS:.c=.o)

TARGET = $(BIN_DIR)/producer_consumer
TEST_BUFFER = $(BIN_DIR)/test_buffer
TEST_CONCURRENCY = $(BIN_DIR)/test_concurrency

.PHONY: all clean test benchmark helgrind memcheck debug

all: $(TARGET) $(TEST_BUFFER) $(TEST_CONCURRENCY)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(TARGET): $(OBJS) $(SRC_DIR)/main.c $(SRC_DIR)/benchmark.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $(OBJS) $(SRC_DIR)/main.c $(SRC_DIR)/benchmark.c -o $@ $(LDFLAGS)

$(TEST_BUFFER): $(SRC_DIR)/buffer.c $(TEST_DIR)/test_buffer.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $(SRC_DIR)/buffer.c $(TEST_DIR)/test_buffer.c -o $@ $(LDFLAGS)

$(TEST_CONCURRENCY): $(OBJS) $(TEST_DIR)/test_concurrency.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $(OBJS) $(TEST_DIR)/test_concurrency.c -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

debug: CFLAGS = $(DEBUG_FLAGS)
debug: clean all

test: all
	@echo "--- Running Unit Tests ---"
	./$(TEST_BUFFER)
	@echo "--- Running Concurrency Tests ---"
	./$(TEST_CONCURRENCY)

benchmark: $(TARGET)
	./$(TARGET) --benchmark

helgrind: debug
	@echo "--- Running Helgrind Thread Sanitizer ---"
	valgrind --tool=helgrind --log-file=helgrind_report.txt ./$(TARGET) -n 50 -v 0
	@echo "Helgrind analysis complete. See helgrind_report.txt"

memcheck: debug
	@echo "--- Running Valgrind Memory Check ---"
	valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all --log-file=memcheck_report.txt ./$(TARGET) -n 50 -v 0
	@echo "Valgrind memcheck complete. See memcheck_report.txt"

clean:
	rm -rf $(SRC_DIR)/*.o $(BIN_DIR)/* helgrind_report.txt memcheck_report.txt

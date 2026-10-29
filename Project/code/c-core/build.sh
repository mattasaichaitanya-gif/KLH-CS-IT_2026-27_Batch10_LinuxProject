#!/bin/bash
set -e

echo "=========================================================="
echo "  Building Concurrent Producer-Consumer System (C Core)"
echo "  KLH - OS & Systems Programming (25CS2104E) - Team 10"
echo "=========================================================="

mkdir -p bin

echo "[1/3] Compiling core application..."
gcc -Wall -Wextra -pthread -O2 -Iinclude -D_POSIX_C_SOURCE=200809L \
    src/buffer.c src/sync.c src/producer_consumer.c src/benchmark.c src/main.c \
    -o bin/producer_consumer -pthread

echo "[2/3] Compiling buffer unit tests..."
gcc -Wall -Wextra -pthread -O2 -Iinclude -D_POSIX_C_SOURCE=200809L \
    src/buffer.c tests/test_buffer.c \
    -o bin/test_buffer -pthread

echo "[3/3] Compiling concurrency edge-case tests..."
gcc -Wall -Wextra -pthread -O2 -Iinclude -D_POSIX_C_SOURCE=200809L \
    src/buffer.c src/sync.c src/producer_consumer.c tests/test_concurrency.c \
    -o bin/test_concurrency -pthread

echo ""
echo "Build succeeded! Executables generated in bin/:"
ls -lh bin/

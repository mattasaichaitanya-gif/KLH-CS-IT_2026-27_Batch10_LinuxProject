#!/bin/bash
set -e

echo "=========================================================="
echo "  Running Helgrind POSIX Thread Safety & Race Detector"
echo "  KLH - OS & Systems Programming (25CS2104E) - Team 10"
echo "=========================================================="

# Build with debug symbols
echo "Compiling with -g for accurate line symbols..."
gcc -Wall -Wextra -pthread -g -O0 -Iinclude -D_POSIX_C_SOURCE=200809L \
    src/buffer.c src/sync.c src/producer_consumer.c src/benchmark.c src/main.c \
    -o bin/producer_consumer_debug -pthread

echo ""
echo "Executing Valgrind Helgrind analysis..."
valgrind --tool=helgrind --log-file=helgrind_report.txt ./bin/producer_consumer_debug -n 30 -v 0

echo ""
echo "=== Helgrind Analysis Summary ==="
grep -E "ERROR SUMMARY|total errors" helgrind_report.txt || true
echo "Full detailed log saved to: c-core/helgrind_report.txt"

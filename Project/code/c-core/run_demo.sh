#!/bin/bash
set -e

echo "=========================================================="
echo "  Running Concurrent Producer-Consumer Demonstration"
echo "  KLH - OS & Systems Programming (25CS2104E) - Team 10"
echo "=========================================================="

# Ensure compiled
if [ ! -f "bin/producer_consumer" ]; then
    echo "Compiling executables..."
    ./build.sh
fi

echo ""
echo "[DEMO 1] Standard Balanced Run (2 Producers, 2 Consumers, Buffer=5, Items=12)"
./bin/producer_consumer -b 5 -p 2 -c 2 -n 12 --p-delay 80 --c-delay 100 -v 1

echo ""
echo "[DEMO 2] Buffer Overflow Avoidance (4 Producers, 1 Consumer, Buffer=3, Items=12)"
./bin/producer_consumer -b 3 -p 4 -c 1 -n 12 --p-delay 20 --c-delay 120 -v 1

echo ""
echo "[DEMO 3] Buffer Underflow Avoidance (1 Producer, 4 Consumers, Buffer=5, Items=12)"
./bin/producer_consumer -b 5 -p 1 -c 4 -n 12 --p-delay 120 --c-delay 20 -v 1

echo ""
echo "All 3 live demonstration scenarios completed successfully!"

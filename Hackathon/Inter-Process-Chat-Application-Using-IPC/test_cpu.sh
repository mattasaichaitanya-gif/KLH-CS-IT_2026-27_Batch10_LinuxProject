#!/bin/bash
set -e
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$PROJECT_DIR"

rm -f /tmp/ipc_chat_cpu_test_*
sleep 10 | ./ipc-chat Alice 1 cpu_test > /dev/null 2>&1 &
P1=$!
sleep 10 | ./ipc-chat Bob 2 cpu_test > /dev/null 2>&1 &
P2=$!

sleep 1
echo "=== Active Process Status & CPU Usage ==="
ps -p "$P1,$P2" -o pid,%cpu,%mem,stat,comm

kill -INT "$P1" "$P2" 2>/dev/null || true
wait "$P1" "$P2" 2>/dev/null || true
rm -f /tmp/ipc_chat_cpu_test_*
echo "=== CPU Test Completed Successfully ==="

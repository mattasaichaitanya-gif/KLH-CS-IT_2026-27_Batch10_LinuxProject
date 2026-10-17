#!/bin/bash
# ==============================================================================
# Script: test.sh
# Description: Full build, verification, and automated live demo for ipc-chat
# ==============================================================================

set -e
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$PROJECT_DIR"

echo "======================================================================="
echo " 1. Building Project with GCC Strict Diagnostics (-Wall -Wextra -Wpedantic)"
echo "======================================================================="
make clean
make all

echo ""
echo "======================================================================="
echo " 2. Running Automated 18-Test Integration Suite"
echo "======================================================================="
./bin/test_runner

echo ""
echo "======================================================================="
echo " 3. Running Live Scripted Two-Terminal IPC Exchange Demo"
echo "======================================================================="

SESSION="demo_session"
rm -f /tmp/ipc_chat_${SESSION}_*

# Launch Alice (Role 1) in background with dedicated stdin pipe
FIFO_ALICE_IN="/tmp/demo_alice_in.fifo"
FIFO_BOB_IN="/tmp/demo_bob_in.fifo"
rm -f "$FIFO_ALICE_IN" "$FIFO_BOB_IN"
mkfifo "$FIFO_ALICE_IN" "$FIFO_BOB_IN"

# Alice process
./ipc-chat Alice 1 "$SESSION" < "$FIFO_ALICE_IN" &
PID_ALICE=$!

# Bob process
./ipc-chat Bob 2 "$SESSION" < "$FIFO_BOB_IN" &
PID_BOB=$!

# Open input feeds
exec 3> "$FIFO_ALICE_IN"
exec 4> "$FIFO_BOB_IN"

sleep 0.5
echo "[Demo Action] Alice sends message to Bob..."
echo "Hello Bob! Testing Linux IPC using POSIX named pipes." >&3
sleep 0.5

echo "[Demo Action] Bob replies to Alice..."
echo "Hi Alice! Full-duplex select() multiplexing works cleanly." >&4
sleep 0.5

echo "[Demo Action] Alice sends message with punctuation..."
echo "Amazing: real-time, non-blocking, zero busy-waiting!" >&3
sleep 0.5

echo "[Demo Action] Bob types /exit to leave chat..."
echo "/exit" >&4
sleep 0.5

# Close feeds
exec 3>&-
exec 4>&-

wait $PID_ALICE 2>/dev/null || true
wait $PID_BOB 2>/dev/null || true
rm -f "$FIFO_ALICE_IN" "$FIFO_BOB_IN" /tmp/ipc_chat_${SESSION}_*

echo ""
echo "======================================================================="
echo " All Tests and Live Demonstrations Completed Successfully!"
echo "======================================================================="

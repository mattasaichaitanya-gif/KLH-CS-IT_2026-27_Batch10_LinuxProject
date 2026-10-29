#!/bin/bash
echo "====================================================================="
echo "  Concurrent Producer-Consumer System Using Semaphores"
echo "  Operating Systems and Systems Programming (25CS2104E) - Team 10"
echo "====================================================================="
echo ""
echo "Starting Web Visualizer Server on http://localhost:3000 ..."
echo ""

# Try opening default browser
if which xdg-open > /dev/null; then
  xdg-open http://localhost:3000 &
elif which gnome-open > /dev/null; then
  gnome-open http://localhost:3000 &
fi

node backend/server.js

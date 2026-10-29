@echo off
title Producer-Consumer System Dashboard - KLH OS (25CS2104E) Team 10
echo =====================================================================
echo   Concurrent Producer-Consumer System Using Semaphores
echo   Operating Systems and Systems Programming (25CS2104E) - Team 10
echo =====================================================================
echo.
echo Starting Web Visualizer Server on http://localhost:3000 ...
echo.
start http://localhost:3000
node backend/server.js
pause

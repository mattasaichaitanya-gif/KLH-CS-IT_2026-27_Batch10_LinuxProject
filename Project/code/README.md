# Concurrent Producer-Consumer System Using Semaphores


Official implementation of the course project for **Operating Systems and Systems Programming (25CS2104E)** at **Koneru Lakshmaiah Education Foundation (KLH)**.

- **GitHub Repository:** [https://github.com/mattasaichaitanya-gif/KLH-CS-IT_2026-27_Batch10_LinuxProject](https://github.com/mattasaichaitanya-gif/KLH-CS-IT_2026-27_Batch10_LinuxProject)
- **Section / Team:** Section No. 10 | Team No. 10


## Project Structure

```
ossp-project/
├── c-core/                         # Native C POSIX Systems Programming Core
│   ├── include/
│   │   ├── common.h                # Color codes, BufferItem, checksum hash, config structs
│   │   ├── buffer.h                # Circular bounded buffer interface (Sujit)
│   │   ├── sync.h                  # POSIX semaphores & mutex API (Bala Charan)
│   │   └── producer_consumer.h     # Thread worker contexts & prototypes (Chaitanya)
│   ├── src/
│   │   ├── buffer.c                # Circular buffer memory allocation & boundary checks
│   │   ├── sync.c                  # Semaphore wait/post logic & thread-safe logger
│   │   ├── producer_consumer.c     # Worker routines & sentinel shutdown protocol
│   │   ├── main.c                  # Interactive CLI application with getopt argument parsing
│   │   └── benchmark.c             # Multi-threaded stress matrix & profiling engine
│   ├── tests/
│   │   ├── test_buffer.c           # Unit tests (wraparound, overflow/underflow defense)
│   │   └── test_concurrency.c      # Concurrency tests (full/empty state blocking)
│   ├── Makefile                    # Standard Makefile (all, clean, test, benchmark, helgrind)
│   ├── build.sh                    # Direct GCC compilation script for Linux/WSL
│   ├── run_demo.sh                 # Quick 3-scenario demo launcher
│   └── run_valgrind.sh             # Helgrind thread-safety test script
│
├── backend/                        # Web Dashboard Backend
│   └── server.js                   # Node.js HTTP, REST API, & Linux C SSE streaming server
│
├── frontend/                       # Interactive Review Dashboard
│   ├── index.html                  # Glassmorphic dark UI dashboard
│   ├── css/
│   │   └── styles.css              # Custom styling, animations, and responsive layout
│   └── js/
│       ├── visualizer.js           # High-DPI canvas circular buffer ring renderer
│       ├── simulation.js           # Discrete-event POSIX simulation engine
│       └── app.js                  # UI event handlers, SSE stream receiver, benchmark charts
│
├── docs/                           # Faculty Review & Viva Documentation
│   ├── PROJECT_REPORT.md           # Formal project report & theoretical background
│   ├── VIVA_QUESTIONS.md           # 25+ OS viva questions & model answers
│   └── INDIVIDUAL_CONTRIBUTIONS.md # Detailed breakdown of work by member
│
├── start.bat                       # 1-Click Windows Web Launcher
├── start.sh                        # 1-Click Linux Web Launcher
└── README.md                       # Repository master documentation
```

---

## Operating System Concepts & Linux APIs Used

| OS Concept / Linux API | Purpose in the Project |
| :--- | :--- |
| `pthread_create` / `pthread_join` | Spawns, runs, and synchronizes producer and consumer worker threads. |
| `sem_init`, `sem_wait`, `sem_post` | Counting semaphores coordinating buffer capacity without CPU busy-waiting. |
| `pthread_mutex_lock`, `unlock` | Enforces mutual exclusion to protect shared circular buffer memory in critical sections. |
| **Circular Bounded Buffer** | Fixed-capacity shared memory FIFO structure with `head` and `tail` pointers. |
| **FNV-1a 32-bit Checksum** | Verifies zero data corruption across concurrent memory operations. |
| **Sentinel Shutdown Pattern** | Injects sentinel items to terminate consumers cleanly without deadlocks. |
| **Valgrind Helgrind** | Intercepts POSIX thread calls to detect race conditions and lock-order deadlocks. |

---

## Quick Start Guide

### 1. Running the Native Linux C Application (CLI)

Ensure you are in a Linux or WSL environment with GCC installed:

```bash
cd c-core

# 1. Compile the complete project
./build.sh

# 2. Run the main application with custom parameters:
# -b 5: buffer capacity 5 slots
# -p 2: 2 producer threads
# -c 2: 2 consumer threads
# -n 20: 20 total items
./bin/producer_consumer -b 5 -p 2 -c 2 -n 20 --p-delay 50 --c-delay 80

# 3. Run all unit and concurrency tests
./bin/test_buffer
./bin/test_concurrency

# 4. Run the automated multi-threaded benchmark suite (10,000 items/scenario)
./bin/producer_consumer --benchmark -j benchmark_results.json
```

### 2. Running Valgrind Helgrind (Thread Safety Verification)

```bash
cd c-core
chmod +x run_valgrind.sh
./run_valgrind.sh
```

**Expected Result:**
```
ERROR SUMMARY: 0 errors from 0 contexts
```

---

## 3. Running the Web Visualizer Dashboard ("Frontend & Backend")

The interactive dashboard allows you to visually explain the circular buffer, semaphore values, and mutex lock state during project reviews.

### Windows:
Double-click `start.bat` or run:
```cmd
node backend/server.js
```
Then open `http://localhost:3000` in your browser.

### Linux / WSL:
```bash
./start.sh
```
Or:
```bash
node backend/server.js
```

### Dashboard Features:
1. **Animated Circular Ring Buffer:** High-DPI canvas showing real-time slot states, particle bursts on insert/extract, and moving `HEAD` and `TAIL` pointer arrows.
2. **Live Counting Semaphores Gauges:** `sem_empty` (tracks available slots) and `sem_full` (tracks filled items).
3. **Binary Mutex Padlock:** Displays `UNLOCKED` or `LOCKED [By Thread]` with critical section state.
4. **Thread Swimlanes:** Live cards showing state transitions for each Producer and Consumer.
5. **Native Linux Execution Mode:** Spawns the compiled Linux C binary in WSL and streams live terminal logs via Server-Sent Events (SSE).
6. **Benchmark Suite Viewer:** Real-time throughput comparison charts across 6 thread concurrency ratios.
7. **Team Review Modal:** Displays institutional details, student roll numbers, and approved individual responsibilities.

---

## Benchmark Results (Throughput Profiling)

| Scenario | Buffer Capacity ($N$) | Producers ($P$) | Consumers ($C$) | Items | Throughput | Data Integrity |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: |
| **1P : 1C** (Baseline) | 10 | 1 | 1 | 10,000 | 66,155 items/s | **0 Errors (PASS)** |
| **1P : 4C** (Consumer Heavy) | 10 | 1 | 4 | 10,000 | 55,930 items/s | **0 Errors (PASS)** |
| **4P : 1C** (Producer Heavy) | 10 | 4 | 1 | 10,000 | 65,349 items/s | **0 Errors (PASS)** |
| **4P : 4C** (Balanced) | 10 | 4 | 4 | 10,000 | 255,636 items/s | **0 Errors (PASS)** |
| **8P : 8C** (High Concurrency) | 20 | 8 | 8 | 10,000 | 386,100 items/s | **0 Errors (PASS)** |
| **16P : 16C** (Peak Contention) | 32 | 16 | 16 | 10,000 | **559,315 items/s** | **0 Errors (PASS)** |

---

## Review Evaluation Checkpoints

- [x] **Mutual Exclusion Verified:** Buffer internal memory strictly protected by `pthread_mutex_t`.
- [x] **Buffer Overflow Avoidance:** Producers strictly blocked via `sem_wait(&empty)` when buffer is full.
- [x] **Buffer Underflow Avoidance:** Consumers strictly blocked via `sem_wait(&full)` when buffer is empty.
- [x] **Deadlock-Free:** Strict lock acquisition hierarchy (Semaphore wait *before* Mutex lock).
- [x] **Zero Busy-Waiting:** Native Linux OS-level sleep/wake-up signaling (`futex`).
- [x] **Data Integrity:** 32-bit FNV-1a checksum verification guarantees 100% corruption-free execution.
- [x] **Valgrind Helgrind Clean:** 0 thread safety errors, 0 lock order violations.

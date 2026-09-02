# Concurrent Producer-Consumer System Using Semaphores

A multi-threaded Linux systems programming project solving the classical producer-consumer synchronization challenge using POSIX threads, counting semaphores, and mutual exclusion locks.

---

## Overview

In multitasking operating systems, managing shared memory among concurrent threads without data corruption is a fundamental challenge. This project implements a thread-safe bounded circular buffer in C. It coordinates multiple producer and consumer threads to prevent race conditions, deadlocks, buffer overflows, and buffer underflows without CPU-intensive busy-waiting.

## Key Features

* **Thread-Safe Bounded Buffer:** Implements a circular queue with head and tail tracking pointers to store shared data items.
* **POSIX Semaphore Synchronization:** Uses counting semaphores (`empty` and `full`) to block producers when the buffer is full and consumers when it is empty.
* **Mutual Exclusion:** Utilizes POSIX mutex locks (`pthread_mutex_t`) to serialize critical section access and protect buffer integrity.
* **Resource Optimization:** Eliminates spinlocks and busy-waiting by relying on OS-level blocking and wake-up notifications.
* **Thread Lifecycle Management:** Spawns and synchronizes concurrent workers with `pthread_create` and `pthread_join`, incorporating simulated processing latencies.

---

## Technical Architecture & Workflow

```text
[ Producers ] ---> sem_wait(&empty) ---> pthread_mutex_lock()
                                              │
                                              ▼
                                 [ Shared Circular Buffer ]
                                              │
                                              ▼
[ Consumers ] <--- sem_wait(&full)  <--- pthread_mutex_unlock()
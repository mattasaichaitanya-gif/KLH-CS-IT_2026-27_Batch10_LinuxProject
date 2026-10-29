/**
 * @file sync.h
 * @brief POSIX Semaphores and Mutex Synchronization Controller
 * @author Duddukuri Bala Charan (Roll No: 2520090087)
 * @course Operating Systems and Systems Programming (25CS2104E) - Section 10, Team 10
 */

#ifndef SYNC_H
#define SYNC_H

#include <semaphore.h>
#include <pthread.h>
#include "common.h"

/**
 * @brief Encapsulates synchronization primitives for bounded buffer coordination
 */
typedef struct {
    sem_t empty;                    /* Counting semaphore: slots available for producers */
    sem_t full;                     /* Counting semaphore: filled slots available for consumers */
    pthread_mutex_t buffer_mutex;   /* Binary lock: guarantees mutual exclusion inside critical section */
    pthread_mutex_t log_mutex;      /* Mutex to ensure atomic, non-interleaved terminal printing */
    int buffer_capacity;            /* Initial buffer capacity N */
    bool is_initialized;
} SyncController;

/**
 * @brief Initializes semaphores and mutexes
 * @param sync Pointer to SyncController instance
 * @param capacity Buffer capacity (N)
 * @return 0 on success, negative error code on failure
 */
int sync_init(SyncController *sync, int capacity);

/**
 * @brief Destroys all semaphores and mutexes, freeing OS synchronization resources
 * @param sync Pointer to SyncController instance
 */
void sync_destroy(SyncController *sync);

/**
 * @brief Producer step 1: Wait on 'empty' semaphore (blocks if buffer is full)
 * @param sync Pointer to SyncController
 * @return 0 on success, -1 on interrupt/error
 */
int sync_wait_empty(SyncController *sync);

/**
 * @brief Producer step 2 & Consumer step 2: Acquire mutex lock for critical section
 * @param sync Pointer to SyncController
 * @return 0 on success
 */
int sync_lock(SyncController *sync);

/**
 * @brief Producer step 4 & Consumer step 4: Release mutex lock exiting critical section
 * @param sync Pointer to SyncController
 * @return 0 on success
 */
int sync_unlock(SyncController *sync);

/**
 * @brief Producer step 5: Post to 'full' semaphore (wakes up waiting consumers)
 * @param sync Pointer to SyncController
 * @return 0 on success
 */
int sync_post_full(SyncController *sync);

/**
 * @brief Consumer step 1: Wait on 'full' semaphore (blocks if buffer is empty)
 * @param sync Pointer to SyncController
 * @return 0 on success, -1 on interrupt/error
 */
int sync_wait_full(SyncController *sync);

/**
 * @brief Consumer step 5: Post to 'empty' semaphore (wakes up waiting producers)
 * @param sync Pointer to SyncController
 * @return 0 on success
 */
int sync_post_empty(SyncController *sync);

/**
 * @brief Retrieves current value of 'empty' semaphore
 * @param sync Pointer to SyncController
 * @return Current integer value of empty semaphore
 */
int sync_get_empty_value(SyncController *sync);

/**
 * @brief Retrieves current value of 'full' semaphore
 * @param sync Pointer to SyncController
 * @return Current integer value of full semaphore
 */
int sync_get_full_value(SyncController *sync);

/**
 * @brief Thread-safe formatted logger protected by log_mutex
 * @param sync Pointer to SyncController
 * @param format Format string
 * @param ... Arguments
 */
void sync_log(SyncController *sync, const char *format, ...);

#endif /* SYNC_H */

/**
 * @file sync.c
 * @brief Implementation of POSIX Semaphores and Mutex Synchronization Controller
 * @author Duddukuri Bala Charan (Roll No: 2520090087)
 * @course Operating Systems and Systems Programming (25CS2104E) - Section 10, Team 10
 * 
 * CORE OS CONCEPTS - SYNCHRONIZATION PRIMITIVES & DEADLOCK PREVENTION:
 * ---------------------------------------------------------------------
 * 1. COUNTING SEMAPHORES (sem_t):
 *    - `empty` starts at buffer capacity N. Tracks available free slots.
 *      Each producer call to `sem_wait(&empty)` decrements the count.
 *      When count reaches 0 (buffer full), producers sleep without burning CPU.
 *    - `full` starts at 0. Tracks available filled items.
 *      Each consumer call to `sem_wait(&full)` decrements the count.
 *      When count reaches 0 (buffer empty), consumers sleep without burning CPU.
 * 
 * 2. MUTUAL EXCLUSION LOCK (pthread_mutex_t):
 *    - Guarantees that only ONE thread (producer or consumer) executes inside
 *      the critical section (manipulating buffer->slots, head, and tail) at any time.
 * 
 * 3. DEADLOCK PREVENTION HIERARCHY RULE:
 *    - ALWAYS wait on the counting semaphore FIRST, then acquire the mutex lock.
 *    - NEVER acquire the mutex lock before waiting on a semaphore.
 *    - Rationale: If a thread held the mutex while sleeping on a zero semaphore,
 *      the opposing thread could never acquire the mutex to produce/consume and
 *      signal the semaphore, locking the entire operating system process permanently!
 */

#include "sync.h"
#include <stdarg.h>
#include <sys/time.h>

/**
 * sync_init:
 * Initializes all counting semaphores and mutex locks for the system.
 * 
 * @param sync: Pointer to SyncController instance to initialize.
 * @param capacity: Maximum buffer slot capacity N.
 * @return: 0 on success, negative error code on failure.
 */
int sync_init(SyncController *sync, int capacity) {
    if (!sync || capacity <= 0) {
        return -1;
    }

    sync->buffer_capacity = capacity;

    /*
     * 1. Initialize 'empty' counting semaphore:
     * - Initial value: N (all slots are initially empty and available for writing).
     * - The second argument (0) specifies that the semaphore is shared between
     *   threads of a single process (not across separate OS processes).
     */
    if (sem_init(&sync->empty, 0, capacity) != 0) {
        perror("sync_init: sem_init(&empty) failed");
        return -2;
    }

    /*
     * 2. Initialize 'full' counting semaphore:
     * - Initial value: 0 (initially, 0 items are available for reading).
     * - Consumers attempting to read immediately will block until producers post items.
     */
    if (sem_init(&sync->full, 0, 0) != 0) {
        perror("sync_init: sem_init(&full) failed");
        sem_destroy(&sync->empty); /* Roll back previous initialization */
        return -3;
    }

    /*
     * 3. Initialize critical section mutex lock:
     * - Protects buffer array index updates (tail and head) and item writing/reading.
     * - NULL attribute specifies default fast mutex behavior.
     */
    if (pthread_mutex_init(&sync->buffer_mutex, NULL) != 0) {
        perror("sync_init: pthread_mutex_init(&buffer_mutex) failed");
        sem_destroy(&sync->empty);
        sem_destroy(&sync->full);
        return -4;
    }

    /*
     * 4. Initialize logging mutex:
     * - Multiple concurrent threads outputting to stdout simultaneously can interleave
     *   their characters, producing garbled terminal lines.
     * - The log_mutex ensures all log writes are atomic and clean.
     */
    if (pthread_mutex_init(&sync->log_mutex, NULL) != 0) {
        perror("sync_init: pthread_mutex_init(&log_mutex) failed");
        sem_destroy(&sync->empty);
        sem_destroy(&sync->full);
        pthread_mutex_destroy(&sync->buffer_mutex);
        return -5;
    }

    sync->is_initialized = true;
    return 0;
}

/**
 * sync_destroy:
 * Cleans up and releases all POSIX kernel synchronization resources.
 */
void sync_destroy(SyncController *sync) {
    if (sync && sync->is_initialized) {
        sem_destroy(&sync->empty);
        sem_destroy(&sync->full);
        pthread_mutex_destroy(&sync->buffer_mutex);
        pthread_mutex_destroy(&sync->log_mutex);
        sync->is_initialized = false;
    }
}

/**
 * sync_wait_empty:
 * Decrements the 'empty' semaphore.
 * If empty count == 0 (buffer full), the calling producer thread enters
 * a non-busy kernel sleep state (Linux futex) until a consumer calls sem_post(&empty).
 * 
 * Includes EINTR handling: if interrupted by an OS signal, it automatically retries.
 */
int sync_wait_empty(SyncController *sync) {
    if (!sync) return -1;
    while (sem_wait(&sync->empty) == -1) {
        if (errno == EINTR) {
            continue; /* Restart semaphore wait if interrupted by an OS signal */
        }
        return -1;
    }
    return 0;
}

/**
 * sync_lock:
 * Acquires mutual exclusion lock before entering the circular buffer critical section.
 * Blocks if another thread currently holds the lock.
 */
int sync_lock(SyncController *sync) {
    if (!sync) return -1;
    return pthread_mutex_lock(&sync->buffer_mutex);
}

/**
 * sync_unlock:
 * Releases mutual exclusion lock upon exiting the critical section,
 * waking up any thread waiting to enter the critical section.
 */
int sync_unlock(SyncController *sync) {
    if (!sync) return -1;
    return pthread_mutex_unlock(&sync->buffer_mutex);
}

/**
 * sync_post_full:
 * Increments the 'full' semaphore.
 * Signals that a new item has been placed in the buffer.
 * If any consumer thread was sleeping in sem_wait(&full), the Linux kernel wakes it up.
 */
int sync_post_full(SyncController *sync) {
    if (!sync) return -1;
    return sem_post(&sync->full);
}

/**
 * sync_wait_full:
 * Decrements the 'full' semaphore.
 * If full count == 0 (buffer empty), the calling consumer thread enters
 * a non-busy kernel sleep state until a producer enqueues an item and calls sem_post(&full).
 */
int sync_wait_full(SyncController *sync) {
    if (!sync) return -1;
    while (sem_wait(&sync->full) == -1) {
        if (errno == EINTR) {
            continue; /* Restart semaphore wait if interrupted by an OS signal */
        }
        return -1;
    }
    return 0;
}

/**
 * sync_post_empty:
 * Increments the 'empty' semaphore.
 * Signals that an item has been dequeued, freeing up a buffer slot.
 * If any producer was sleeping in sem_wait(&empty), the Linux kernel wakes it up.
 */
int sync_post_empty(SyncController *sync) {
    if (!sync) return -1;
    return sem_post(&sync->empty);
}

/**
 * sync_get_empty_value:
 * Reads current value of the 'empty' semaphore without modifying it.
 */
int sync_get_empty_value(SyncController *sync) {
    if (!sync) return -1;
    int val = 0;
    sem_getvalue(&sync->empty, &val);
    return val;
}

/**
 * sync_get_full_value:
 * Reads current value of the 'full' semaphore without modifying it.
 */
int sync_get_full_value(SyncController *sync) {
    if (!sync) return -1;
    int val = 0;
    sem_getvalue(&sync->full, &val);
    return val;
}

/**
 * sync_log:
 * Thread-safe logging function that atomically prints formatted strings to stdout.
 * Uses `log_mutex` to prevent concurrent threads from interleaving text on console.
 */
void sync_log(SyncController *sync, const char *format, ...) {
    if (!sync) return;

    /* Acquire log mutex lock to serialize terminal output */
    pthread_mutex_lock(&sync->log_mutex);

    /* Generate precise sub-second timestamp [HH:MM:SS.mmm] */
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm *tm_info = localtime(&tv.tv_sec);

    char time_str[32];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);

    printf(ANSI_DIM "[%s.%03ld]" ANSI_RESET " ", time_str, tv.tv_usec / 1000);

    /* Format variable argument list */
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    printf("\n");
    fflush(stdout);

    /* Release log mutex lock */
    pthread_mutex_unlock(&sync->log_mutex);
}

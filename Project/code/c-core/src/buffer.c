/**
 * @file buffer.c
 * @brief Thread-Safe Circular Bounded Buffer Implementation
 * @author Vakkalanka Sai Bhaskara Sujit (Roll No: 2520090085)
 * @course Operating Systems and Systems Programming (25CS2104E) - Section 10, Team 10
 * 
 * CORE OS CONCEPT - CIRCULAR BOUNDED BUFFER:
 * ------------------------------------------
 * A bounded buffer is a fixed-capacity FIFO queue stored in contiguous memory.
 * Rather than shifting items left upon each dequeue (an expensive O(N) operation),
 * the buffer maintains two integer pointers:
 *   1. `tail` (Write Pointer): where the next produced item is inserted.
 *   2. `head` (Read Pointer): from where the next consumed item is extracted.
 * Both pointers advance using modulo arithmetic: `ptr = (ptr + 1) % capacity`.
 * When a pointer reaches the end of the array, it seamlessly wraps around to index 0.
 */

#include "buffer.h"

/**
 * buffer_create:
 * Allocates and initializes the CircularBuffer structure and its slot array.
 * 
 * @param capacity: Maximum number of BufferItems the buffer can hold (N > 0).
 * @return: Pointer to initialized CircularBuffer on success, NULL on invalid capacity or OOM.
 */
CircularBuffer* buffer_create(int capacity) {
    /* Step 1: Validate input parameters */
    if (capacity <= 0 || capacity > MAX_BUFFER_CAPACITY) {
        return NULL;
    }

    /* Step 2: Allocate the primary control structure */
    CircularBuffer *buf = (CircularBuffer *)malloc(sizeof(CircularBuffer));
    if (!buf) {
        perror("buffer_create: failed to allocate CircularBuffer struct");
        return NULL;
    }

    /* Step 3: Allocate zeroed memory for the slot array of size N */
    buf->slots = (BufferItem *)calloc(capacity, sizeof(BufferItem));
    if (!buf->slots) {
        perror("buffer_create: failed to allocate buffer slots array");
        free(buf); /* Clean up allocated control structure before returning */
        return NULL;
    }

    /* Step 4: Initialize state variables and telemetry counters */
    buf->capacity = capacity;       /* Fixed size N */
    buf->head = 0;                   /* Consumer starts reading from index 0 */
    buf->tail = 0;                   /* Producer starts writing to index 0 */
    buf->count = 0;                  /* Initially, 0 items stored */
    buf->peak_count = 0;             /* High-water mark of buffer occupancy */
    buf->total_enqueued = 0;         /* Cumulative enqueue count */
    buf->total_dequeued = 0;         /* Cumulative dequeue count */

    return buf;
}

/**
 * buffer_destroy:
 * Frees all allocated memory to prevent memory leaks in long-running systems.
 * 
 * @param buf: Pointer to CircularBuffer instance to deallocate.
 */
void buffer_destroy(CircularBuffer *buf) {
    if (buf) {
        if (buf->slots) {
            free(buf->slots);
            buf->slots = NULL; /* Prevent dangling pointer */
        }
        free(buf);
    }
}

/**
 * buffer_enqueue:
 * Inserts an item into the buffer at the current `tail` index.
 * 
 * NOTE ON CONCURRENCY:
 * This function modifies shared state (`slots`, `tail`, `count`).
 * The calling thread MUST acquire `pthread_mutex_lock` BEFORE calling this function.
 * 
 * @param buf: Pointer to CircularBuffer instance.
 * @param item: The BufferItem to insert.
 * @param out_slot: Optional output pointer to receive the slot index written to.
 * @return: 0 on success, -1 if buffer is full (overflow prevented).
 */
int buffer_enqueue(CircularBuffer *buf, BufferItem item, int *out_slot) {
    if (!buf || !buf->slots) {
        return -1;
    }

    /* 
     * Defensive Boundary Safety Guard:
     * Under correct POSIX semaphore usage, sem_wait(&empty) guarantees count < capacity.
     * However, this check enforces structural defense-in-depth:
     * If an external bug bypassed semaphores, this assertion prevents memory corruption.
     */
    if (buf->count >= buf->capacity) {
        return -1; /* Buffer overflow strictly prevented */
    }

    /* Write item into the current write slot */
    int write_idx = buf->tail;
    buf->slots[write_idx] = item;
    
    if (out_slot) {
        *out_slot = write_idx;
    }

    /* 
     * Circular pointer advancement:
     * Uses modulo arithmetic to wrap tail pointer back to 0 when it exceeds (capacity - 1).
     * Example with capacity = 5:
     *   tail 0 -> 1 -> 2 -> 3 -> 4 -> (5 % 5 = 0)
     */
    buf->tail = (buf->tail + 1) % buf->capacity;
    buf->count++;
    buf->total_enqueued++;

    /* Update peak occupancy metric for performance profiling */
    if (buf->count > buf->peak_count) {
        buf->peak_count = buf->count;
    }

    return 0; /* Enqueue successful */
}

/**
 * buffer_dequeue:
 * Extracts an item from the buffer from the current `head` index.
 * 
 * NOTE ON CONCURRENCY:
 * This function modifies shared state (`slots`, `head`, `count`).
 * The calling thread MUST acquire `pthread_mutex_lock` BEFORE calling this function.
 * 
 * @param buf: Pointer to CircularBuffer instance.
 * @param out_item: Pointer to store the extracted BufferItem.
 * @param out_slot: Optional output pointer to receive the slot index read from.
 * @return: 0 on success, -1 if buffer is empty (underflow prevented).
 */
int buffer_dequeue(CircularBuffer *buf, BufferItem *out_item, int *out_slot) {
    if (!buf || !buf->slots || !out_item) {
        return -1;
    }

    /* 
     * Defensive Boundary Safety Guard:
     * sem_wait(&full) guarantees count > 0 before dequeueing.
     * This check provides structural protection against buffer underflow.
     */
    if (buf->count <= 0) {
        return -1; /* Buffer underflow strictly prevented */
    }

    /* Extract item from the current read slot */
    int read_idx = buf->head;
    *out_item = buf->slots[read_idx];

    if (out_slot) {
        *out_slot = read_idx;
    }

    /* Clear the slot memory for security and clean state debugging */
    memset(&buf->slots[read_idx], 0, sizeof(BufferItem));

    /* 
     * Circular pointer advancement:
     * Advances head index, wrapping to 0 when reaching the end of the array.
     */
    buf->head = (buf->head + 1) % buf->capacity;
    buf->count--;
    buf->total_dequeued++;

    return 0; /* Dequeue successful */
}

/**
 * buffer_is_empty:
 * Checks whether the buffer currently contains 0 items.
 */
int buffer_is_empty(const CircularBuffer *buf) {
    return (buf != NULL && buf->count == 0);
}

/**
 * buffer_is_full:
 * Checks whether the buffer currently has reached its maximum capacity N.
 */
int buffer_is_full(const CircularBuffer *buf) {
    return (buf != NULL && buf->count == buf->capacity);
}

/**
 * buffer_get_count:
 * Returns the instantaneous number of occupied slots in the buffer.
 */
int buffer_get_count(const CircularBuffer *buf) {
    return buf ? buf->count : 0;
}

/**
 * buffer_format_state:
 * Generates an ASCII string visualization of the buffer slots and pointer locations.
 * Format: [ H:#1 | #2 | T:_ | _ ] (2/4 slots)
 * Useful for command-line tracing and log inspection.
 */
void buffer_format_state(const CircularBuffer *buf, char *dest, size_t dest_size) {
    if (!buf || !dest || dest_size == 0) return;

    dest[0] = '\0';
    char temp[64];

    snprintf(temp, sizeof(temp), "[");
    strncat(dest, temp, dest_size - strlen(dest) - 1);

    for (int i = 0; i < buf->capacity; i++) {
        if (i == buf->head && i == buf->tail && buf->count == 0) {
            snprintf(temp, sizeof(temp), " H/T:_ ");
        } else if (i == buf->head && i == buf->tail && buf->count == buf->capacity) {
            snprintf(temp, sizeof(temp), " H/T:#%d ", buf->slots[i].id);
        } else if (i == buf->head) {
            if (buf->slots[i].id > 0)
                snprintf(temp, sizeof(temp), " H:#%d ", buf->slots[i].id);
            else
                snprintf(temp, sizeof(temp), " H:_ ");
        } else if (i == buf->tail) {
            if (buf->slots[i].id > 0)
                snprintf(temp, sizeof(temp), " T:#%d ", buf->slots[i].id);
            else
                snprintf(temp, sizeof(temp), " T:_ ");
        } else {
            if (buf->slots[i].id > 0)
                snprintf(temp, sizeof(temp), " #%d ", buf->slots[i].id);
            else
                snprintf(temp, sizeof(temp), " _ ");
        }

        strncat(dest, temp, dest_size - strlen(dest) - 1);
        if (i < buf->capacity - 1) {
            strncat(dest, "|", dest_size - strlen(dest) - 1);
        }
    }

    snprintf(temp, sizeof(temp), "] (%d/%d slots)", buf->count, buf->capacity);
    strncat(dest, temp, dest_size - strlen(dest) - 1);
}

/**
 * buffer_verify_item_integrity:
 * Validates that an item was not corrupted during transit through the buffer.
 * Recomputes the FNV-1a checksum and verifies it matches the item's checksum field.
 * 
 * @return: 1 if checksum matches (data valid), 0 if mismatch (corruption detected).
 */
int buffer_verify_item_integrity(const BufferItem *item) {
    if (!item || item->id < 0) {
        return 0;
    }
    uint32_t expected_hash = calculate_checksum(item->id, item->producer_id, item->value, item->timestamp_ms);
    return (expected_hash == item->checksum);
}

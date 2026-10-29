/**
 * @file buffer.h
 * @brief Thread-Safe Circular Bounded Buffer Specification
 * @author Vakkalanka Sai Bhaskara Sujit (Roll No: 2520090085)
 * @course Operating Systems and Systems Programming (25CS2104E) - Section 10, Team 10
 */

#ifndef BUFFER_H
#define BUFFER_H

#include "common.h"

/**
 * @brief Fixed-size Circular Bounded Buffer structure
 */
typedef struct {
    BufferItem *slots;          /* Dynamically allocated array of BufferItems */
    int capacity;               /* Maximum number of slots (N) */
    int head;                   /* Read index for consumer extraction */
    int tail;                   /* Write index for producer insertion */
    int count;                  /* Current number of filled slots */
    int peak_count;             /* High-water mark of buffer occupancy */
    long long total_enqueued;   /* Total items successfully enqueued */
    long long total_dequeued;   /* Total items successfully dequeued */
} CircularBuffer;

/**
 * @brief Allocates and initializes the circular bounded buffer
 * @param capacity Maximum number of items the buffer can store (N > 0)
 * @return Pointer to initialized CircularBuffer, or NULL on memory failure
 */
CircularBuffer* buffer_create(int capacity);

/**
 * @brief Deallocates all resources associated with the circular buffer
 * @param buf Pointer to the CircularBuffer instance
 */
void buffer_destroy(CircularBuffer *buf);

/**
 * @brief Inserts an item into the circular buffer at the current tail index
 * @param buf Pointer to the CircularBuffer instance
 * @param item The BufferItem to insert
 * @param out_slot Optional pointer to receive the slot index written to
 * @return 0 on success, -1 if buffer is full (boundary error)
 */
int buffer_enqueue(CircularBuffer *buf, BufferItem item, int *out_slot);

/**
 * @brief Removes and retrieves an item from the circular buffer at the current head index
 * @param buf Pointer to the CircularBuffer instance
 * @param out_item Pointer to store the extracted BufferItem
 * @param out_slot Optional pointer to receive the slot index read from
 * @return 0 on success, -1 if buffer is empty (boundary error)
 */
int buffer_dequeue(CircularBuffer *buf, BufferItem *out_item, int *out_slot);

/**
 * @brief Checks if the buffer is currently empty
 * @param buf Pointer to the CircularBuffer instance
 * @return 1 if empty (count == 0), 0 otherwise
 */
int buffer_is_empty(const CircularBuffer *buf);

/**
 * @brief Checks if the buffer is currently full
 * @param buf Pointer to the CircularBuffer instance
 * @return 1 if full (count == capacity), 0 otherwise
 */
int buffer_is_full(const CircularBuffer *buf);

/**
 * @brief Returns the current number of items in the buffer
 * @param buf Pointer to the CircularBuffer instance
 * @return Current count
 */
int buffer_get_count(const CircularBuffer *buf);

/**
 * @brief Renders an ASCII visual representation of the buffer state
 * @param buf Pointer to the CircularBuffer instance
 * @param dest Buffer to write the visual representation into
 * @param dest_size Size of destination string buffer
 */
void buffer_format_state(const CircularBuffer *buf, char *dest, size_t dest_size);

/**
 * @brief Validates an item's data integrity against its embedded checksum
 * @param item Pointer to the BufferItem to verify
 * @return 1 if data is intact and valid, 0 if data corruption detected
 */
int buffer_verify_item_integrity(const BufferItem *item);

#endif /* BUFFER_H */

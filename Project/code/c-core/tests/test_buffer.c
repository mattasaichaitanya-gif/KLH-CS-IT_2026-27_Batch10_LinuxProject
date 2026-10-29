/**
 * @file test_buffer.c
 * @brief Unit tests for circular bounded buffer boundary conditions and wrap-around
 * @author Vakkalanka Sai Bhaskara Sujit (Roll No: 2520090085)
 * @course Operating Systems and Systems Programming (25CS2104E) - Section 10, Team 10
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "../include/common.h"
#include "../include/buffer.h"

static void test_initialization(void) {
    printf("[TEST] Testing buffer allocation and initialization... ");
    CircularBuffer *buf = buffer_create(5);
    assert(buf != NULL);
    assert(buf->capacity == 5);
    assert(buf->count == 0);
    assert(buf->head == 0);
    assert(buf->tail == 0);
    assert(buffer_is_empty(buf) == 1);
    assert(buffer_is_full(buf) == 0);
    buffer_destroy(buf);
    printf("PASSED\n");
}

static void test_enqueue_dequeue(void) {
    printf("[TEST] Testing normal enqueue and dequeue... ");
    CircularBuffer *buf = buffer_create(3);
    assert(buf != NULL);

    BufferItem item1 = { .id = 101, .producer_id = 1, .value = 42, .timestamp_ms = 1000, .checksum = 0 };
    item1.checksum = calculate_checksum(item1.id, item1.producer_id, item1.value, item1.timestamp_ms);

    int slot = -1;
    int res = buffer_enqueue(buf, item1, &slot);
    assert(res == 0);
    assert(slot == 0);
    assert(buffer_get_count(buf) == 1);
    assert(buffer_is_empty(buf) == 0);

    BufferItem out_item;
    int out_slot = -1;
    res = buffer_dequeue(buf, &out_item, &out_slot);
    assert(res == 0);
    assert(out_slot == 0);
    assert(out_item.id == 101);
    assert(out_item.value == 42);
    assert(buffer_verify_item_integrity(&out_item) == 1);
    assert(buffer_is_empty(buf) == 1);

    buffer_destroy(buf);
    printf("PASSED\n");
}

static void test_overflow_prevention(void) {
    printf("[TEST] Testing overflow prevention (boundary defense)... ");
    CircularBuffer *buf = buffer_create(2);

    BufferItem i1 = { .id = 1, .producer_id = 1, .value = 10, .timestamp_ms = 1000, .checksum = 0 };
    BufferItem i2 = { .id = 2, .producer_id = 1, .value = 20, .timestamp_ms = 1000, .checksum = 0 };
    BufferItem i3 = { .id = 3, .producer_id = 1, .value = 30, .timestamp_ms = 1000, .checksum = 0 };

    assert(buffer_enqueue(buf, i1, NULL) == 0);
    assert(buffer_enqueue(buf, i2, NULL) == 0);
    assert(buffer_is_full(buf) == 1);

    /* Third enqueue must fail and NOT corrupt existing data */
    assert(buffer_enqueue(buf, i3, NULL) == -1);
    assert(buffer_get_count(buf) == 2);

    buffer_destroy(buf);
    printf("PASSED\n");
}

static void test_underflow_prevention(void) {
    printf("[TEST] Testing underflow prevention (boundary defense)... ");
    CircularBuffer *buf = buffer_create(2);
    assert(buffer_is_empty(buf) == 1);

    BufferItem out;
    /* Dequeue on empty buffer must fail */
    assert(buffer_dequeue(buf, &out, NULL) == -1);

    buffer_destroy(buf);
    printf("PASSED\n");
}

static void test_circular_wraparound(void) {
    printf("[TEST] Testing circular wrap-around indexing... ");
    CircularBuffer *buf = buffer_create(3);

    for (int cycle = 1; cycle <= 10; cycle++) {
        BufferItem in = { .id = cycle, .producer_id = 1, .value = cycle * 10, .timestamp_ms = 1000, .checksum = 0 };
        in.checksum = calculate_checksum(in.id, in.producer_id, in.value, in.timestamp_ms);
        assert(buffer_enqueue(buf, in, NULL) == 0);

        BufferItem out;
        assert(buffer_dequeue(buf, &out, NULL) == 0);
        assert(out.id == cycle);
        assert(buffer_verify_item_integrity(&out) == 1);
    }

    assert(buffer_is_empty(buf) == 1);
    buffer_destroy(buf);
    printf("PASSED\n");
}

int main(void) {
    printf("\n========================================\n");
    printf("  RUNNING CIRCULAR BUFFER UNIT TESTS\n");
    printf("========================================\n");

    test_initialization();
    test_enqueue_dequeue();
    test_overflow_prevention();
    test_underflow_prevention();
    test_circular_wraparound();

    printf("\n[SUCCESS] ALL UNIT TESTS PASSED SUCCESSFULLY!\n\n");
    return 0;
}

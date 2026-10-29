/**
 * @file test_concurrency.c
 * @brief Concurrency edge-case validation and race-condition stress test
 * @author Vakkalanka Sai Bhaskara Sujit (Roll No: 2520090085)
 * @course Operating Systems and Systems Programming (25CS2104E) - Section 10, Team 10
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "../include/common.h"
#include "../include/buffer.h"
#include "../include/sync.h"
#include "../include/producer_consumer.h"

static void test_producer_blocking_when_full(void) {
    printf("[CONCURRENCY TEST] Testing Producer blocking when buffer full... ");

    /* 1-capacity buffer, 2 producers, 0 consumers initially */
    SystemConfig cfg = {
        .buffer_capacity = 2,
        .num_producers = 4,
        .num_consumers = 2,
        .total_items = 20,
        .producer_delay_ms = 10,
        .consumer_delay_ms = 20,
        .verbose_level = 0,
        .benchmark_mode = false,
        .json_output_file = ""
    };

    BenchmarkResult res;
    int rc = run_system_orchestration(&cfg, &res);
    assert(rc == 0);
    assert(res.items_produced == 20);
    assert(res.items_consumed == 20);
    assert(res.items_corrupted == 0);
    assert(res.peak_buffer_occupancy <= cfg.buffer_capacity);

    printf("PASSED (Peak buffer: %d/%d)\n", res.peak_buffer_occupancy, cfg.buffer_capacity);
}

static void test_consumer_blocking_when_empty(void) {
    printf("[CONCURRENCY TEST] Testing Consumer blocking when buffer empty... ");

    /* Fast consumers, slow producers */
    SystemConfig cfg = {
        .buffer_capacity = 5,
        .num_producers = 1,
        .num_consumers = 4,
        .total_items = 25,
        .producer_delay_ms = 20,
        .consumer_delay_ms = 5,
        .verbose_level = 0,
        .benchmark_mode = false,
        .json_output_file = ""
    };

    BenchmarkResult res;
    int rc = run_system_orchestration(&cfg, &res);
    assert(rc == 0);
    assert(res.items_produced == 25);
    assert(res.items_consumed == 25);
    assert(res.items_corrupted == 0);

    printf("PASSED (Elapsed: %.2fs)\n", res.elapsed_seconds);
}

int main(void) {
    printf("\n============================================\n");
    printf("  RUNNING CONCURRENCY EDGE-CASE TESTS\n");
    printf("============================================\n");

    test_producer_blocking_when_full();
    test_consumer_blocking_when_empty();

    printf("\n[SUCCESS] ALL CONCURRENCY EDGE-CASE TESTS PASSED!\n\n");
    return 0;
}

/**
 * @file producer_consumer.h
 * @brief Thread Worker Routines and Lifecycle Management
 * @author Matta Sai Chaitanya (Roll No: 2520090023)
 * @course Operating Systems and Systems Programming (25CS2104E) - Section 10, Team 10
 */

#ifndef PRODUCER_CONSUMER_H
#define PRODUCER_CONSUMER_H

#include "buffer.h"
#include "sync.h"

#define SENTINEL_ITEM_ID -1

/**
 * @brief Context passed to each Producer thread
 */
typedef struct {
    int thread_id;              /* Logical producer ID: 1, 2, ... */
    int start_item_id;          /* Starting sequence ID */
    int items_to_produce;       /* Number of items this thread must generate */
    int delay_ms;               /* Simulated workload delay in ms */
    CircularBuffer *buffer;     /* Shared circular buffer */
    SyncController *sync;       /* Shared synchronization primitives */
    SystemConfig *config;       /* Pointer to configuration */
    long long items_produced;   /* Total items successfully produced */
    long long wait_time_us;     /* Time spent waiting on semaphores */
} ProducerContext;

/**
 * @brief Context passed to each Consumer thread
 */
typedef struct {
    int thread_id;              /* Logical consumer ID: 1, 2, ... */
    int delay_ms;               /* Simulated consumption workload delay in ms */
    CircularBuffer *buffer;     /* Shared circular buffer */
    SyncController *sync;       /* Shared synchronization primitives */
    SystemConfig *config;       /* Pointer to configuration */
    long long items_consumed;   /* Total items successfully consumed */
    long long wait_time_us;     /* Time spent waiting on semaphores */
    int integrity_errors;       /* Count of corrupted items detected */
} ConsumerContext;

/**
 * @brief Producer thread worker routine
 * @param arg Pointer to ProducerContext
 * @return NULL on thread exit
 */
void* producer_worker(void *arg);

/**
 * @brief Consumer thread worker routine
 * @param arg Pointer to ConsumerContext
 * @return NULL on thread exit
 */
void* consumer_worker(void *arg);

/**
 * @brief Spawns producer & consumer threads, orchestrates execution, joins threads
 * @param config Simulation parameters
 * @param result Output benchmark and metrics summary
 * @return 0 on success, negative error code on failure
 */
int run_system_orchestration(const SystemConfig *config, BenchmarkResult *result);

#endif /* PRODUCER_CONSUMER_H */

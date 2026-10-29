/**
 * @file producer_consumer.c
 * @brief Thread Worker Routines and Lifecycle Implementation
 * @author Matta Sai Chaitanya (Roll No: 2520090023)
 * @course Operating Systems and Systems Programming (25CS2104E) - Section 10, Team 10
 * 
 * CORE OS CONCEPTS - THREAD WORKER LIFECYCLE & PROTOCOLS:
 * -------------------------------------------------------
 * 1. PRODUCER SYNCHRONIZATION PROTOCOL:
 *    [Step 1] Compute/Generate data item & FNV-1a checksum.
 *    [Step 2] sem_wait(&empty)           -> Decrement free slot count (blocks if buffer full).
 *    [Step 3] pthread_mutex_lock(&mutex) -> Lock shared buffer memory (Critical Section).
 *    [Step 4] buffer_enqueue(item)       -> Write to tail, update pointer: tail = (tail+1)%N.
 *    [Step 5] pthread_mutex_unlock(&mutex)-> Release buffer lock.
 *    [Step 6] sem_post(&full)            -> Increment full count, wake waiting consumer.
 * 
 * 2. CONSUMER SYNCHRONIZATION PROTOCOL:
 *    [Step 1] sem_wait(&full)            -> Decrement filled slot count (blocks if buffer empty).
 *    [Step 2] pthread_mutex_lock(&mutex) -> Lock shared buffer memory (Critical Section).
 *    [Step 3] buffer_dequeue(&item)      -> Read from head, update pointer: head = (head+1)%N.
 *    [Step 4] pthread_mutex_unlock(&mutex)-> Release buffer lock.
 *    [Step 5] sem_post(&empty)           -> Increment empty count, wake waiting producer.
 *    [Step 6] Check for Sentinel ID      -> Clean loop exit on termination signal.
 *    [Step 7] Verify FNV-1a Checksum     -> Zero data race / corruption verification.
 * 
 * 3. SENTINEL SHUTDOWN PATTERN:
 *    To terminate consumers without abrupt cancellations (`pthread_cancel`), the orchestrator
 *    enqueues N_consumers special sentinel items (ID = -1). Each consumer extracts one sentinel
 *    and exits naturally, allowing clean `pthread_join` without resource leaks.
 */

#include "producer_consumer.h"
#include <sys/time.h>

/**
 * get_micro_time:
 * Monotonic high-resolution microsecond timer for precise benchmarking.
 */
static long long get_micro_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000LL + (ts.tv_nsec / 1000);
}

/**
 * producer_worker:
 * Execution routine for producer threads spawned via pthread_create.
 * Generates partitioned quota of items, coordinating with semaphores and mutex.
 */
void* producer_worker(void *arg) {
    ProducerContext *ctx = (ProducerContext *)arg;
    CircularBuffer *buf = ctx->buffer;
    SyncController *sync = ctx->sync;
    const SystemConfig *cfg = ctx->config;

    for (int i = 0; i < ctx->items_to_produce; i++) {
        /* [Step 1] Simulate artificial workload production latency */
        if (ctx->delay_ms > 0) {
            sleep_ms(ctx->delay_ms);
        }

        /* Generate deterministic data payload & integrity checksum */
        int item_seq = ctx->start_item_id + i;
        int payload_val = (item_seq * 17 + ctx->thread_id * 31) % 1000;
        long long ts = current_time_ms();
        uint32_t csum = calculate_checksum(item_seq, ctx->thread_id, payload_val, ts);

        BufferItem item;
        item.id = item_seq;
        item.producer_id = ctx->thread_id;
        item.consumer_id = 0; /* Populated upon dequeue */
        item.value = payload_val;
        item.timestamp_ms = ts;
        item.checksum = csum;

        if (cfg->verbose_level >= 2) {
            int empty_val = sync_get_empty_value(sync);
            sync_log(sync, ANSI_COLOR_CYAN "[PRODUCER %d]" ANSI_RESET 
                     " Requesting empty slot... (sem_empty count: %d)", 
                     ctx->thread_id, empty_val);
        }

        /* 
         * [Step 2] Wait on counting semaphore 'empty':
         * If buffer is full (count == capacity), this thread blocks in kernel sleep.
         * We measure wait duration for performance profiling.
         */
        long long wait_start = get_micro_time();
        sync_wait_empty(sync);
        ctx->wait_time_us += (get_micro_time() - wait_start);

        /* 
         * [Step 3] Acquire mutual exclusion lock (Critical Section entry):
         * Enforces that only 1 thread manipulates the circular buffer array at once.
         */
        sync_lock(sync);

        /* 
         * [Step 4] Enqueue item into shared circular buffer:
         * Writes item into buf->slots[tail] and advances tail = (tail + 1) % N.
         */
        int written_slot = -1;
        int status = buffer_enqueue(buf, item, &written_slot);

        if (status == 0) {
            ctx->items_produced++;
            if (cfg->verbose_level >= 1) {
                char state_str[256];
                buffer_format_state(buf, state_str, sizeof(state_str));
                sync_log(sync, ANSI_COLOR_GREEN "[PRODUCER %d]" ANSI_RESET 
                         " -> Produced item " ANSI_BOLD "#%-3d" ANSI_RESET 
                         " [Val: %3d] at Slot %d | %s",
                         ctx->thread_id, item.id, item.value, written_slot, state_str);
            }
        } else {
            sync_log(sync, ANSI_COLOR_RED "[ERROR][PRODUCER %d] Buffer overflow prevented!" ANSI_RESET, ctx->thread_id);
        }

        /* 
         * [Step 5] Release mutual exclusion lock (Critical Section exit):
         * Critical section is kept strictly minimal for maximum throughput.
         */
        sync_unlock(sync);

        /* 
         * [Step 6] Signal counting semaphore 'full':
         * Increments full count by 1. Wakes up any consumer thread sleeping on sem_wait(&full).
         */
        sync_post_full(sync);
    }

    if (cfg->verbose_level >= 2) {
        sync_log(sync, ANSI_COLOR_CYAN "[PRODUCER %d]" ANSI_RESET " Finished all %d items. Thread exiting.", 
                 ctx->thread_id, ctx->items_to_produce);
    }

    return NULL;
}

/**
 * consumer_worker:
 * Execution routine for consumer threads spawned via pthread_create.
 * Runs in a continuous loop extracting items until receiving a sentinel shutdown signal.
 */
void* consumer_worker(void *arg) {
    ConsumerContext *ctx = (ConsumerContext *)arg;
    CircularBuffer *buf = ctx->buffer;
    SyncController *sync = ctx->sync;
    const SystemConfig *cfg = ctx->config;

    while (1) {
        if (cfg->verbose_level >= 2) {
            int full_val = sync_get_full_value(sync);
            sync_log(sync, ANSI_COLOR_MAGENTA "[CONSUMER %d]" ANSI_RESET 
                     " Waiting for full slot... (sem_full count: %d)", 
                     ctx->thread_id, full_val);
        }

        /* 
         * [Step 1] Wait on counting semaphore 'full':
         * If buffer is empty (count == 0), this thread blocks in kernel sleep.
         */
        long long wait_start = get_micro_time();
        sync_wait_full(sync);
        ctx->wait_time_us += (get_micro_time() - wait_start);

        /* 
         * [Step 2] Acquire mutual exclusion lock (Critical Section entry):
         * Enforces exclusive access to buffer array while reading and updating head.
         */
        sync_lock(sync);

        /* 
         * [Step 3] Dequeue item from shared circular buffer:
         * Extracts item from buf->slots[head] and advances head = (head + 1) % N.
         */
        BufferItem item;
        int read_slot = -1;
        int status = buffer_dequeue(buf, &item, &read_slot);

        char state_str[256] = "";
        if (status == 0 && cfg->verbose_level >= 1) {
            buffer_format_state(buf, state_str, sizeof(state_str));
        }

        /* 
         * [Step 4] Release mutual exclusion lock (Critical Section exit):
         */
        sync_unlock(sync);

        /* 
         * [Step 5] Signal counting semaphore 'empty':
         * Increments empty count by 1. Wakes up any producer thread sleeping on sem_wait(&empty).
         */
        sync_post_empty(sync);

        /* 
         * [Step 6] Sentinel Termination Check:
         * If the extracted item has ID = -1, the system orchestrator has signaled clean shutdown.
         * The consumer immediately exits its loop without resource leaks.
         */
        if (item.id == SENTINEL_ITEM_ID) {
            if (cfg->verbose_level >= 2) {
                sync_log(sync, ANSI_COLOR_YELLOW "[CONSUMER %d]" ANSI_RESET " Received shutdown sentinel. Exiting cleanly.", ctx->thread_id);
            }
            break;
        }

        if (status == 0) {
            ctx->items_consumed++;
            item.consumer_id = ctx->thread_id;

            /* 
             * [Step 7] Verify data integrity against FNV-1a checksum:
             * Guarantees zero race conditions or memory clobbering occurred in transit.
             */
            if (!buffer_verify_item_integrity(&item)) {
                ctx->integrity_errors++;
                sync_log(sync, ANSI_COLOR_RED "[CORRUPTION DETECTED] Item #%d failed checksum verification!" ANSI_RESET, item.id);
            } else if (cfg->verbose_level >= 1) {
                sync_log(sync, ANSI_COLOR_YELLOW "[CONSUMER %d]" ANSI_RESET 
                         " <- Consumed item " ANSI_BOLD "#%-3d" ANSI_RESET 
                         " [from Prod %d, Val: %3d] from Slot %d | %s",
                         ctx->thread_id, item.id, item.producer_id, item.value, read_slot, state_str);
            }
        }

        /* [Step 8] Simulate consumption processing workload */
        if (ctx->delay_ms > 0) {
            sleep_ms(ctx->delay_ms);
        }
    }

    return NULL;
}

/**
 * run_system_orchestration:
 * Coordinates the full multi-threaded lifecycle:
 *   1. Allocates shared bounded buffer memory structure.
 *   2. Initializes counting semaphores and mutex locks.
 *   3. Spawns consumer worker threads first (so they are ready to receive).
 *   4. Spawns producer worker threads with partitioned item quotas.
 *   5. Waits for all producer threads to finish (`pthread_join`).
 *   6. Injects sentinel termination items (one per consumer).
 *   7. Waits for all consumer threads to finish (`pthread_join`).
 *   8. Computes throughput and validates zero errors.
 */
int run_system_orchestration(const SystemConfig *config, BenchmarkResult *result) {
    if (!config || !result) return -1;

    memset(result, 0, sizeof(BenchmarkResult));

    /* Step 1: Create Circular Bounded Buffer of capacity N */
    CircularBuffer *buf = buffer_create(config->buffer_capacity);
    if (!buf) {
        fprintf(stderr, "Fatal: failed to allocate circular buffer with capacity %d\n", config->buffer_capacity);
        return -1;
    }

    /* Step 2: Initialize Semaphores and Mutex Locks */
    SyncController sync;
    if (sync_init(&sync, config->buffer_capacity) != 0) {
        fprintf(stderr, "Fatal: failed to initialize POSIX semaphores\n");
        buffer_destroy(buf);
        return -2;
    }

    pthread_t prod_threads[MAX_THREADS];
    pthread_t cons_threads[MAX_THREADS];
    ProducerContext prod_ctx[MAX_THREADS];
    ConsumerContext cons_ctx[MAX_THREADS];

    int num_p = config->num_producers;
    int num_c = config->num_consumers;
    int total_items = config->total_items;

    /* Distribute total workload evenly across all producer threads */
    int base_items_per_prod = total_items / num_p;
    int extra_items = total_items % num_p;
    int curr_seq = 1;

    /* Record start timestamp for benchmarking */
    long long start_mono_us = get_micro_time();

    /* Step 3: Spawn Consumer Threads First */
    for (int i = 0; i < num_c; i++) {
        cons_ctx[i].thread_id = i + 1;
        cons_ctx[i].delay_ms = config->consumer_delay_ms;
        cons_ctx[i].buffer = buf;
        cons_ctx[i].sync = &sync;
        cons_ctx[i].config = (SystemConfig *)config;
        cons_ctx[i].items_consumed = 0;
        cons_ctx[i].wait_time_us = 0;
        cons_ctx[i].integrity_errors = 0;

        if (pthread_create(&cons_threads[i], NULL, consumer_worker, &cons_ctx[i]) != 0) {
            perror("pthread_create consumer failed");
            return -3;
        }
    }

    /* Step 4: Spawn Producer Threads */
    for (int i = 0; i < num_p; i++) {
        int items_for_this = base_items_per_prod + (i < extra_items ? 1 : 0);
        prod_ctx[i].thread_id = i + 1;
        prod_ctx[i].start_item_id = curr_seq;
        prod_ctx[i].items_to_produce = items_for_this;
        prod_ctx[i].delay_ms = config->producer_delay_ms;
        prod_ctx[i].buffer = buf;
        prod_ctx[i].sync = &sync;
        prod_ctx[i].config = (SystemConfig *)config;
        prod_ctx[i].items_produced = 0;
        prod_ctx[i].wait_time_us = 0;

        curr_seq += items_for_this;

        if (pthread_create(&prod_threads[i], NULL, producer_worker, &prod_ctx[i]) != 0) {
            perror("pthread_create producer failed");
            return -4;
        }
    }

    /* Step 5: Wait for all Producer Threads to Complete (pthread_join) */
    for (int i = 0; i < num_p; i++) {
        pthread_join(prod_threads[i], NULL);
    }

    /* 
     * Step 6: Graceful Consumer Shutdown
     * Inject num_c Sentinel items (ID = SENTINEL_ITEM_ID = -1) into the buffer
     * so each consumer consumes one sentinel and terminates without hanging.
     */
    for (int i = 0; i < num_c; i++) {
        sync_wait_empty(&sync);
        sync_lock(&sync);

        BufferItem sentinel;
        sentinel.id = SENTINEL_ITEM_ID;
        sentinel.producer_id = 0;
        sentinel.consumer_id = 0;
        sentinel.value = 0;
        sentinel.timestamp_ms = 0;
        sentinel.checksum = 0;
        buffer_enqueue(buf, sentinel, NULL);

        sync_unlock(&sync);
        sync_post_full(&sync);
    }

    /* Step 7: Wait for all Consumer Threads to Complete (pthread_join) */
    for (int i = 0; i < num_c; i++) {
        pthread_join(cons_threads[i], NULL);
    }

    long long end_mono_us = get_micro_time();

    /* Step 8: Aggregate Metrics */
    result->elapsed_seconds = (double)(end_mono_us - start_mono_us) / 1000000.0;
    result->peak_buffer_occupancy = buf->peak_count;

    for (int i = 0; i < num_p; i++) {
        result->items_produced += prod_ctx[i].items_produced;
        result->total_wait_time_empty_us += prod_ctx[i].wait_time_us;
    }

    for (int i = 0; i < num_c; i++) {
        result->items_consumed += cons_ctx[i].items_consumed;
        result->total_wait_time_full_us += cons_ctx[i].wait_time_us;
        result->items_corrupted += cons_ctx[i].integrity_errors;
    }

    if (result->elapsed_seconds > 0) {
        result->throughput_items_per_sec = (double)result->items_consumed / result->elapsed_seconds;
    }

    /* Cleanup resources */
    sync_destroy(&sync);
    buffer_destroy(buf);

    return 0;
}


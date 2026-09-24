#include <stdio.h>
#include <pthread.h>

#include "producer.h"
#include "consumer.h"
#include "sync.h"

#define NUM_PRODUCERS 2
#define NUM_CONSUMERS 2

int main()
{
    pthread_t producers[NUM_PRODUCERS];
    pthread_t consumers[NUM_CONSUMERS];

    int producer_ids[NUM_PRODUCERS] = {1, 2};
    int consumer_ids[NUM_CONSUMERS] = {1, 2};

    // Initialize semaphores and mutex
    init_sync();

    printf("====================================\n");
    printf("   PRODUCER-CONSUMER SYSTEM\n");
    printf("====================================\n\n");

    // Create producer threads
    for (int i = 0; i < NUM_PRODUCERS; i++)
    {
        pthread_create(
            &producers[i],
            NULL,
            producer,
            &producer_ids[i]
        );
    }

    // Create consumer threads
    for (int i = 0; i < NUM_CONSUMERS; i++)
    {
        pthread_create(
            &consumers[i],
            NULL,
            consumer,
            &consumer_ids[i]
        );
    }

    // Wait for producer threads
    for (int i = 0; i < NUM_PRODUCERS; i++)
    {
        pthread_join(producers[i], NULL);
    }

    // Wait for consumer threads
    for (int i = 0; i < NUM_CONSUMERS; i++)
    {
        pthread_join(consumers[i], NULL);
    }

    // Destroy semaphores and mutex
    destroy_sync();

    printf("\n====================================\n");
    printf("   PROGRAM COMPLETED\n");
    printf("====================================\n");

    return 0;
}
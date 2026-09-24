#include <stdio.h>
#include <pthread.h>

#include "consumer.h"
#include "buffer.h"
#include "sync.h"

void *consumer(void *arg)
{
    int id = *(int *)arg;

    for (int i = 1; i <= 5; i++)
    {
        // Wait for an available item
        sem_wait(&full);

        // Lock the shared buffer
        pthread_mutex_lock(&mutex);

        // Remove item from buffer
        int item = remove_item();

        printf("Consumer %d consumed %d\n", id, item);

        // Unlock the shared buffer
        pthread_mutex_unlock(&mutex);

        // Tell producers that an empty slot is available
        sem_post(&empty);
    }

    return NULL;
}
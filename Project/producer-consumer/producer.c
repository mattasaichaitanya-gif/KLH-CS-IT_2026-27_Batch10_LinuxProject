#include <stdio.h>
#include <pthread.h>

#include "producer.h"
#include "buffer.h"
#include "sync.h"

void *producer(void *arg)
{
    int id = *(int *)arg;

    for (int i = 1; i <= 5; i++)
    {
        int item = i + (id * 100);

        // Wait for an empty slot
        sem_wait(&empty);

        // Lock the shared buffer
        pthread_mutex_lock(&mutex);

        // Insert item into buffer
        insert_item(item);

        printf("Producer %d produced %d\n", id, item);

        // Unlock the shared buffer
        pthread_mutex_unlock(&mutex);

        // Tell consumers that an item is available
        sem_post(&full);
    }

    return NULL;
}
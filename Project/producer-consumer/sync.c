#include "sync.h"
#include "buffer.h"

sem_t empty;
sem_t full;

pthread_mutex_t mutex;

void init_sync(void)
{
    // Initially all buffer positions are empty
    sem_init(&empty, 0, BUFFER_SIZE);

    // Initially there are no items
    sem_init(&full, 0, 0);

    // Initialize mutex
    pthread_mutex_init(&mutex, NULL);
}

void destroy_sync(void)
{
    sem_destroy(&empty);
    sem_destroy(&full);

    pthread_mutex_destroy(&mutex);
}
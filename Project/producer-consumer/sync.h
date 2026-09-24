#ifndef SYNC_H
#define SYNC_H

#include <semaphore.h>
#include <pthread.h>

extern sem_t empty;
extern sem_t full;

extern pthread_mutex_t mutex;

void init_sync(void);
void destroy_sync(void);

#endif

#include "../include/rmalloc/cache.h"
#include "../include/rmalloc/extent.h"
#include "../include/rmalloc/pool.h"
#include <pthread.h>
#include "../include/rmalloc/slab.h"

extern uint32_t sizes[NUM_CACHES];


void init_pool(pool *p)
{
    list_init(&p->global);
    list_init(&p->large);
    list_init(&p->tracked);
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutex_init(&p->lock, &attr);
    pthread_mutexattr_destroy(&attr);
    for(uint8_t i = 0; i != NUM_CACHES; ++i)
        init_cache(&p->slabs[i], p, sizes[i], i);
}
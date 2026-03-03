#include "cache.h"
#include "extent.h"
#include "pool.h"
#include <pthread.h>
#include "slab.h"

extern uint32_t sizes[NUM_CACHES];

/**
 * @brief       Initializes a pool.
 * 
 * @param p     Pool.
 */
void init_pool(pool *p)
{
    list_init(&p->global);
    list_init(&p->large);
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutex_init(&p->lock, &attr);
    pthread_mutexattr_destroy(&attr);
    for(uint8_t i = 0; i != NUM_CACHES; ++i)
        init_cache(&p->slabs[i], p, sizes[i], i);
}
#include "../include/rmalloc/god.h"
#include "../include/rmalloc/recycle.h"
#include "../include/rmalloc/superblock.h"
#include <stdio.h>
extern uint8_t setup;

void init_god(god *g)
{
    uint8_t exp = 0;
    if(atomic_compare_exchange_strong(&g->init, &exp, 1)){
        setup = 1;
        atomic_init(&g->active, 0);
        atomic_init(&g->counter, 0);
        list_init(&g->heaps);
        pthread_mutexattr_t mattr;
        pthread_mutexattr_init(&mattr);
        pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_DEFAULT);
        pthread_mutex_init(&g->lock, &mattr);
        pthread_mutexattr_destroy(&mattr);
        init_superblock(&g->sb, NULL, GOD);
        setup = 2;
        pthread_attr_t tattr;
        pthread_attr_init(&tattr);
        pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
        /*create background recycling thread*/
        g->rs =  pthread_create(&g->janitor, &tattr, release_memory, NULL);
        setup = 3;
    }
}
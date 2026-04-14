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
        default_init_mutex(&g->lock);
        init_superblock(&g->sb, NULL, GOD);
        setup = 2;
        g->rs =   create_detachable_thread(&g->janitor, release_memory, NULL);
        setup = 3;
    }
}
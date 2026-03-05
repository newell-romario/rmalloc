#include "../include/rmalloc/bin.h"

void init_bin(bin *b)
{
    stack_init(&b->global);
    stack *bins = b->bins;
    for(uint8_t  i = 0; i < NUM_CACHES; ++i, ++bins){
        stack_init(bins);
        atomic_init(&b->caches[i].frag, 0);
        atomic_init(&b->caches[i].inuse, 0);
        atomic_init(&b->caches[i].capacity, 0);
        atomic_init(&b->caches[i].tslabs, 0);
        atomic_init(&b->caches[i].eslabs, 0);
    }
}
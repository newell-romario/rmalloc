#ifndef CACHE_H_
#define CACHE_H_
#include "types.h"
void  init_cache(cache *, pool *, size_t, uint8_t);
void  recover_all_slabs(pool *);
void  recover_slabs(cache *);
#endif
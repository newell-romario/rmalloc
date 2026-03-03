#ifndef _RECYCLE_H_
#define _RECYCLE_H_
#include "types.h"
void release_superblock();
void release_memory_from_global();
void release_large_slabs(superblock *);
void* release_memory(void *);
void dump_normal_slabs_from_superblock(superblock *);
void dump_normal_slabs_from_bins();
#endif
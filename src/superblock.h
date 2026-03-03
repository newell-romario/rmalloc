
#ifndef _SUPERBLOCK_H_
#define _SUPERBLOCK_H_
#include <stdint.h>
#include "types.h"
#define   SHRINK_THRESHOLD    .60

void init_superblock(superblock *, sb_stats *, uint8_t); 
uint8_t* align_allocate(superblock *, size_t, size_t);
uint8_t* allocate_object(superblock *, size_t );
void deallocate_object(size_t, uint8_t *);
void* shrink_expand(superblock *, void *, size_t);
size_t  allocation_size(uint8_t *);
void clean_up_slabs(superblock *);
#endif
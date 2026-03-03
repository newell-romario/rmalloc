
#ifndef __MALLOC_H_
#define __MALLOC_H_
#include "types.h"
size_t _msize(void *);
void*  malloc(size_t);
void*  calloc(size_t, size_t);
void*  realloc(void*, size_t);
void   free(void *);
int    posix_memalign(void**, size_t, size_t);
void*  aligned_alloc(size_t, size_t);
void*  valloc(size_t);
void*  pvalloc(size_t);
void*  memalign(size_t, size_t);
void*  reallocarray(void *, size_t, size_t);
size_t malloc_usable_size(void *);
void   local_stats(sb_stats *);
void   global_stats(sb_stats *);
void* rmalloc(size_t);
void* rcalloc(size_t, size_t);
void* rrealloc(void *, size_t);
void* raligned_alloc(size_t, size_t);
void* rmemalign(size_t, size_t);
void* rvalloc(size_t);
void* rpvalloc(size_t);
int   rposix_memalign(void **, size_t, size_t);
void  rfree(void *);
void* raligned_malloc(size_t, size_t);
void  raligned_free(void *);
void* raligned_realloc(void *, size_t, size_t);
void* rreallocarray(void *, size_t, size_t);
#endif
#ifndef RMALLOC_H_
#define RMALLOC_H_
#include <stddef.h>
typedef struct hs{
struct{
size_t  osize;
size_t  malloc;
size_t  free;
size_t  remote;
size_t  local;
size_t  frag;
size_t  inuse;
size_t  capacity;
size_t  peak;
}cache_stats[52];
size_t    lactive;
size_t    linuse;
size_t    lcapacity;
size_t    malloc;
size_t    free;
size_t    remote;
size_t    local;
size_t    frag;
size_t    inuse;
size_t    capacity;
size_t    peak;
size_t    tslabs;
size_t    orphaned;
size_t    released;
size_t    requested;
}hs;

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
size_t rmsize(void *);
void   rlocal_stats(hs *);
void   rglobal_stats(hs *);



/*Libc Malloc Api*/
size_t msize(void *);
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

#endif
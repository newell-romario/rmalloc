#include "../include/rmalloc.h"
#include "../include/rmalloc/types.h"
#include "../include/rmalloc/malloc.h"
#include "../include/rmalloc/superblock.h"
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include "../include/rmalloc/util.h"
#include <stddef.h>
#include "../include/rmalloc/stats.h"

god creator;
bin recycle;
g_stats gs;
pthread_key_t key;
pthread_once_t once = PTHREAD_ONCE_INIT;
thread_local superblock* heap = NULL;
thread_local size_t sk = SIZE_MAX;
_Atomic(size_t) timer  = 0;
volatile uint8_t setup = 0;



static inline superblock* thread_local_superblock();
static inline void  allocate_superblock(); 


/**
 * @brief               Creates thread local superblock or returns already 
 *                      create thread local superblock.
 * 
 * @return superblock*  Superblock.
 */
__attribute__((always_inline))
static inline superblock* thread_local_superblock()
{
    if(r_likely(heap != NULL))
        return heap;
    if(setup == 0)
        pthread_once(&once, init);
    do{
        /**
         * @brief   If we made it this far then setup should be >= 1. 
         *          If setup is < 1 that means we have more than one thread
         *          wanting to access memory simutaneously and the god superblock
         *          isn't yet initialized. In this case one of the threads must be
         *          initializing the god superblock. In that case we sit 
         *          patiently in this loop waiting for the initialization to be complete
         *          before allocating a new superblock.
         */
    }while(setup < 2);
    pthread_mutex_lock(&creator.lock);
    allocate_superblock();
    pthread_mutex_unlock(&creator.lock);
    return heap;
}

static void   allocate_superblock()
{
    heap = (superblock *)allocate_object(&creator.sb, SBSIZE);
    if(r_likely(heap != NULL)){
        #ifdef STATS
            sb_stats *stat =  (sb_stats *)allocate_object(&creator.sb, SBS_SIZE);
            if(r_unlikely(stat == NULL)){
                deallocate_object(creator.sb.sk, (uint8_t*)heap);
                heap = NULL;
            }else{
                init_superblock(heap, stat, NORMAL);  
                list_enqueue(&creator.heaps, &heap->next);
                sk = heap->sk;
                atomic_fetch_add_explicit(&creator.active, 1, memory_order_relaxed);
                pthread_setspecific(key, (void *)heap);
            }   
        #else 
            init_superblock(heap, NULL, NORMAL);
            list_enqueue(&creator.heaps, &heap->next);
            sk = heap->sk;
            atomic_fetch_add_explicit(&creator.active, 1, memory_order_relaxed);
            pthread_setspecific(key, (void *)heap);
        #endif
    }
}


inline void* rmalloc(size_t size)
{
    superblock *sb = thread_local_superblock();
    return allocate_object(sb, size);
}


inline void rfree(void *obj)
{
    if(r_likely(obj != NULL))
        deallocate_object(sk, obj);
}



inline void* rcalloc(size_t nelem, size_t size)
{
    size_t prod = unsigned_multiplication_overflow(nelem, size);
    if(r_likely(prod != 0)){
        superblock *sb = thread_local_superblock();
        uint8_t* obj   = allocate_object(sb, prod);
        if(r_likely(obj != NULL))
            memset(obj, 0, prod);
        return obj;
    }
    return NULL;
}



inline void* rrealloc(void *obj, size_t size)
{
    if(r_unlikely(obj == NULL))
        return rmalloc(size);
    
    if(r_unlikely(size == 0)){
        rfree(obj);
        return obj;
    }
    superblock *sb = thread_local_superblock();
    return shrink_expand(sb, obj, size);
}




inline void* rmemalign(size_t alignment, size_t size)
{
    uint8_t *obj = NULL;
    if(r_likely(is_power_of_two(alignment))){
        superblock *sb = thread_local_superblock();
        obj  = align_allocate(sb, size, alignment); 
    }else errno = EINVAL;
    return obj;
}



inline int rposix_memalign(void **ptr, size_t alignment, size_t size)
{
    int error = 0;
    if(r_likely(is_power_of_two(alignment) && alignment%WORD_SIZE == 0)){
        superblock *sb = thread_local_superblock();
        uint8_t *obj = align_allocate(sb, size, alignment);
        if(r_likely(obj != NULL)){
            *ptr = obj;
            return error;
        }   
        error = ENOMEM;
    }else{
        error = EINVAL;
        *ptr = NULL;
    };   
    return error;
}


inline void* raligned_alloc(size_t alignment, size_t size)
{
    if(r_likely(is_power_of_two(alignment) && size % alignment == 0)){
        superblock *sb = thread_local_superblock();
        return align_allocate(sb, size, alignment);
    }
    return NULL;
}



inline void* rvalloc(size_t size)
{
    return rmemalign(page_size, size);
}



inline void* rpvalloc(size_t size)
{
    size_t osize = try_round_up(size, page_size);
    return rmemalign(page_size, osize);
    
    return NULL;
}


inline void* raligned_malloc(size_t size, size_t alignment)
{
    return rmemalign(alignment, size);
}



inline void raligned_free(void *ptr)
{
    rfree(ptr);
}


inline size_t rmsize(void *ptr)
{
    return allocation_size(ptr);
}


inline void*   raligned_realloc(void *ptr, size_t size, size_t alignment)
{
    if(r_unlikely(ptr != NULL && size == 0)){
        rfree(ptr);
        return NULL;
    }
        
    size_t asize = rmsize(ptr);
    size_t osize = asize <= size? asize: size;
    uint8_t *obj    = rmemalign(alignment, size);
    if(r_likely(obj != NULL)){
        memcpy(obj, ptr, osize);
        rfree(ptr);
    }
    return obj;
}



inline void* rreallocarray(void *ptr, size_t nelem, size_t size)
{
    size_t prod = unsigned_multiplication_overflow(nelem, size);
    if(r_likely(prod != 0))
        return rrealloc(ptr, prod);
    return NULL;
}
#ifdef STATS
/**
 * @brief       Returns stats specific to this superblock.
 * 
 * @param ts 
 */
void local_stats(sb_stats *ts)
{
    superblock *sb = thread_local_superblock();
    
        *ts = *sb->stat;
  
}
 

/**
 * @brief       Aggregates the statistics of all superblocks.
 * 
 * @param ts    Heap stats.
 */
void rglobal_stats(sb_stats *ts)
{
 
        init_stat(ts);
        pthread_mutex_lock(&creator.lock);
        listnode *head = &creator.heaps;
        listnode *cur  = list_first(head);
        sb_stats *stat = NULL;
        superblock *sb = NULL;
        for(;cur != NULL && cur != head; cur = cur->next){
            sb   = container_of(cur, superblock, next);
            stat = sb->stat;
            for(uint8_t i = 0; i < NUM_CACHES; ++i){
                ts->caches[i].malloc    += stat->caches[i].malloc;
                ts->caches[i].free      += stat->caches[i].free;
                ts->caches[i].remote    += stat->caches[i].remote;
                ts->caches[i].local     += stat->caches[i].local;
                ts->caches[i].frag      += stat->caches[i].frag;
                ts->caches[i].inuse     += stat->caches[i].inuse;
                ts->caches[i].capacity  += stat->caches[i].capacity;
            }
            
            ts->lactive     += stat->lactive;
            ts->linuse      += stat->linuse;
            ts->lcapacity   += stat->lcapacity;
            ts->malloc      += stat->malloc;
            ts->free        += stat->free;
            ts->remote      += stat->remote;
            ts->local       += stat->local; 
            ts->frag        += stat->frag;
            ts->inuse       += stat->inuse;
            ts->capacity    += stat->capacity;
            ts->tslabs      += stat->tslabs;
            ts->orphaned    += stat->orphaned;
            ts->requested   += stat->requested;
        }
        pthread_mutex_unlock(&creator.lock);

        
        ts->malloc +=  atomic_load_explicit(&gs.malloc, memory_order_relaxed);
        ts->free   +=  atomic_load_explicit(&gs.free, memory_order_relaxed);
        ts->remote +=  atomic_load_explicit(&gs.remote, memory_order_relaxed);
        ts->local  +=  atomic_load_explicit(&gs.local, memory_order_relaxed);
        ts->requested += atomic_load_explicit(&gs.requested, memory_order_relaxed);
        ts->released += atomic_load_explicit(&gs.released, memory_order_relaxed);
        ts->frag += atomic_load_explicit(&recycle.frag, memory_order_relaxed);
        ts->inuse += atomic_load_explicit(&recycle.inuse, memory_order_relaxed);
        ts->capacity += atomic_load_explicit(&recycle.capacity, memory_order_relaxed);
        ts->tslabs += atomic_load_explicit(&recycle.tslabs, memory_order_relaxed);
        ts->linuse += atomic_load_explicit(&recycle.linuse, memory_order_relaxed);
        ts->lactive += atomic_load_explicit(&recycle.lactive, memory_order_relaxed);
        ts->lcapacity += atomic_load_explicit(&recycle.lcapacity, memory_order_relaxed);
        for(uint8_t i = 0; i < NUM_CACHES; ++i){
            ts->caches[i].frag    += atomic_load_explicit(&recycle.caches[i].frag,
                                        memory_order_relaxed);
            ts->caches[i].inuse  += atomic_load_explicit(&recycle.caches[i].inuse, 
                                        memory_order_relaxed);
            ts->caches[i].capacity += atomic_load_explicit(&recycle.caches[i].capacity,
                                        memory_order_relaxed);
        }

        if(ts->capacity > atomic_load_explicit(&gs.peak, memory_order_relaxed)){
            ts->peak = ts->capacity;
            atomic_store_explicit(&gs.peak, ts->capacity, memory_order_relaxed);
        }else{
            ts->peak = atomic_load_explicit(&gs.peak, memory_order_relaxed);
        }


        for(uint8_t i = 0; i < NUM_CACHES; ++i){
            if(ts->caches[i].capacity > 
                atomic_load_explicit(&gs.peaks[i].peak, memory_order_relaxed)){
                ts->caches[i].peak = ts->caches[i].capacity;
                atomic_store_explicit(&gs.peaks[i].peak, ts->caches[i].peak, memory_order_relaxed);
            }else{
                ts->caches[i].peak  =  atomic_load_explicit(&gs.peaks[i].peak, 
                    memory_order_relaxed);
            }
        }

}
#endif


/**Libc Malloc API*/


void* malloc(size_t size)
{
    return rmalloc(size);
}

void* calloc(size_t nelem, size_t size)
{
    return rcalloc(nelem, size);
}

void* realloc(void *ptr, size_t size)
{
    return rrealloc(ptr, size);
}

inline void free(void *ptr)
{
    rfree(ptr);
}   

int posix_memalign(void **ptr, size_t alignment, size_t size)
{
    return rposix_memalign(ptr, alignment, size);
}

void* aligned_alloc(size_t alignment, size_t size)
{
    return raligned_alloc(alignment, size);
}

void* valloc(size_t size)
{
    return rvalloc(size);
}


void* pvalloc(size_t size)
{
    return rpvalloc(size);
}

void* memalign(size_t alignment, size_t size)
{
    return rmemalign(alignment, size);
}

size_t malloc_usable_size(void *ptr)
{
    if(ptr == NULL)
        return 0;
    return rmsize(ptr);
}

void* reallocarray(void *ptr, size_t nelem, size_t size)
{
    return rreallocarray(ptr, nelem, size);
}
#include "bin.h"
#include <errno.h>
#include "god.h"
#include "recycle.h"
#include <stdio.h>
#include <string.h>
#include "superblock.h"
#include "stats.h"
#include "types.h"
#include "util.h"
#include <sys/mman.h>

static inline void abandon(void *);
static inline superblock* thread_local_superblock();
static inline void   init();
static inline void* _malloc(size_t);
static inline void* _calloc(size_t, size_t);
static inline void* _realloc(void *, size_t);
static inline void* _aligned_alloc(size_t, size_t);
static inline void* _memalign(size_t, size_t);
static inline void* _valloc(size_t);
static inline void* _pvalloc(size_t);
static inline int   _posix_memalign(void **, size_t, size_t);
static void  _free(void *);
static inline void* _aligned_malloc(size_t, size_t);
static inline void  _aligned_free(void *);
static inline void* _aligned_realloc(void *, size_t, size_t);
static inline void* _reallocarray(void *, size_t, size_t);
static inline void  allocate_superblock(); 


god creator;
bin recycle;
g_stats gs;
pthread_key_t key;
pthread_once_t once = PTHREAD_ONCE_INIT;
static thread_local superblock* heap = NULL;
static thread_local size_t sk = SIZE_MAX;
_Atomic(size_t) timer  = 0;
volatile uint8_t setup = 0;

/**
 * @brief           Initializes the entire memory allocator.
 * 
 */
__attribute__((constructor)) void init()
{
    pthread_key_create(&key, abandon);    
    init_god(&creator);
    init_bin(&recycle);
    init_gs(&gs);
}



/**
 * @brief       Adds an abandon superblock to the abandoned list.
 * 
 * @param ptr   Superblock.
 */
static inline void abandon(void *ptr)
{
    superblock *sb = (superblock *)ptr;
    sb->status = ABANDONED;
    clean_up_slabs(sb);
    #ifdef STATS
        dump_stats(sb->stat);
    #endif
}

/**
 * @brief           Allocates a block of memory.
 * 
 * @param size      Size.
 * @return void*    Returns pointer to the new block of memory, else NULL.
 */
__attribute__((always_inline))
static inline void* _malloc(size_t size)
{
    superblock *sb = thread_local_superblock();
    return allocate_object(sb, size);
}



/**
 * @brief       Frees a block of memory allocated.
 * 
 * @param obj   Object.
 */
__attribute__((always_inline))
static inline void _free(void *obj)
{
    if(__builtin_expect(obj != NULL, 0))
        deallocate_object(sk, obj);
}

/**
 * @brief           Allocates a block of memory of at least nelem*size in bytes.
 *                  Additionally, zeroes the memory.
 * 
 * @param nelem     Number of elements.
 * @param size      Size.
 * @return void*    Returns a pointer to the new the block of memory, else NULL.
 */
__attribute__((always_inline))
void* _calloc(size_t nelem, size_t size)
{
    size_t prod = unsigned_multiplication_overflow(nelem, size);
    if(prod != 0){
        superblock *sb = thread_local_superblock();
        uint8_t* obj   = allocate_object(sb, prod);
        if(obj != NULL)
            memset(obj, 0, prod);
        return obj;
    }
    return NULL;
}

/**
 * @brief               Attempts to expand or shrink or free a block of memory
 *                      by size bytes.
 * 
 * @param obj           Object.
 * @param size          Size.
 * @return void*        Returns a pointer to the start of the new block of memory,
 *                      else NULL.
 */
__attribute__((always_inline))
void* _realloc(void *obj, size_t size)
{
    if(obj == NULL)
        return _malloc(size);
    
    if(size == 0){
        _free(obj);
        return obj;
    }
    superblock *sb = thread_local_superblock();
    return shrink_expand(sb, obj, size);
}


/**
 * @brief               Allocates a block of memory of at least size bytes 
 *                      and ensures the block of memory starts on a mulitple
 *                      of alignment. N.B Alignment must be a power of 2.
 * 
 * @param alignment     Alignment.
 * @param size          Size.
 * @return void*        Returns a pointer to the block of memory, else NULL.
 */
__attribute__((always_inline))
void*   _memalign(size_t alignment, size_t size)
{
    uint8_t *obj = NULL;
    if(is_power_of_two(alignment)){
        superblock *sb = thread_local_superblock();
        obj  = align_allocate(sb, size, alignment); 
    }else errno = EINVAL;
    return obj;
}

/**
 * @brief               Allocates a block of memory of at least size bytes and
 *                      ensures the block of memory starts on a multiple of 
 *                      alignment. N.B Alignment must be a power of two and a
 *                      multiple of sizeof(void*).
 * 
 * @param ptr           Pointer.
 * @param alignment     Alignment.
 * @param size          Size.
 * @return int          Returns 0 zero on success, else non zero value.
 */
__attribute__((always_inline))
int _posix_memalign(void **ptr, size_t alignment, size_t size)
{
    int error = 0;
    if(is_power_of_two(alignment) && alignment%WORD_SIZE == 0){
        superblock *sb = thread_local_superblock();
        uint8_t *obj = align_allocate(sb, size, alignment);
        if(obj != NULL){
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

/**
 * @brief           Allocates a block of memeory of at least size bytes and
 *                  ensures the block of memory starts on a multiple of 
 *                  alignment. Alignment must be a power of 2 and size must
 *                  be an multiple of alignment.
 * 
 * @param alignment Alignment.
 * @param size      Size.
 * @return void*    Returns pointer to the block of aligned memory, else NULL.
 */
__attribute__((always_inline))
void* _aligned_alloc(size_t alignment, size_t size)
{
    if(is_power_of_two(alignment) && size % alignment == 0){
        superblock *sb = thread_local_superblock();
        return align_allocate(sb, size, alignment);
    }
    return NULL;
}

/**
 * @brief           Allocates a block of memory of at least size bytes and 
 *                  ensures block of memory is paged aligned.
 * 
 * @param size      Size.
 * @return void*    Returns pointer to page aligned block of memory, else NULL.
 */
__attribute__((always_inline))
void* _valloc(size_t size)
{
    return _memalign(PAGE_SIZE, size);
}

/**
 * @brief           Allocates a block of memory of at least try_round_up(size, PAGE_SIZE)
 *                  size.
 * 
 * @param size      Size.
 * @return void*    Returns pointer to page aligned block of memory, else NULL.
 */
__attribute__((always_inline))
void* _pvalloc(size_t size)
{
    size_t osize = try_round_up(size, PAGE_SIZE);
    if(osize >= size && size != 0)
        return _memalign(PAGE_SIZE, osize);
    
    return NULL;
}

/**
 * @brief               Windows version of memalign.
 * 
 * @param size          Size.
 * @param alignment     Alignment.
 * @return void*        Returns pointer to the new block of memory, else NULL.
 */
__attribute__((always_inline))
void* _aligned_malloc(size_t size, size_t alignment)
{
    return _memalign(alignment, size);
}

/**
 * @brief       Frees an object allocated by _aligned_malloc.
 * 
 * @param ptr   Pointer.
 */
__attribute__((always_inline))
void _aligned_free(void *ptr)
{
    _free(ptr);
}

/**
 * @brief           Finds the size of the memory block passed.
 * 
 * @param ptr       Ptr.
 * @return size_t   Returns the size of memory block.
 */
size_t _msize(void *ptr)
{
    return allocation_size(ptr);
}


/**
 * @brief           Realloc function that maintains alignment.
 * 
 * @param ptr       Pointer.
 * @param size      Size.
 * @param alignment Alignment.
 * @return void*    Returns new aligned block of memory, else NULL.
 */
__attribute__((always_inline))
void*   _aligned_realloc(void *ptr, size_t size, size_t alignment)
{
    if(ptr != NULL && size == 0){
        _free(ptr);
        return NULL;
    }
        
    size_t asize = _msize(ptr);
    size_t osize = asize <= size? asize: size;
    uint8_t *obj    = _memalign(alignment, size);
    if(obj != NULL){
        memcpy(obj, ptr, osize);
        _free(ptr);
    }
    return obj;
}

/**
 * @brief           The same as realloc only difference is that we get to 
 *                  choose how many elements we want to expand/shrink the
 *                  the block of memory by.
 * 
 * @param ptr       Pointer.
 * @param nelem     Nelem.
 * @param size      Size.
 * @return void*    Returns a pointer to the new block of memory, else NULL.
 */
__attribute__((always_inline))
void* _reallocarray(void *ptr, size_t nelem, size_t size)
{
    size_t prod = unsigned_multiplication_overflow(nelem, size);
    if(prod != 0)
        return _realloc(ptr, prod);
    return NULL;
}

/**
 * @brief               Creates thread local superblock or returns already 
 *                      create thread local superblock.
 * 
 * @return superblock*  Superblock.
 */
__attribute__((always_inline))
static inline superblock* thread_local_superblock()
{
    if(__builtin_expect(heap != NULL, 1))
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


/**
 * @brief  Allocates a new superblock using the god superblock.
 *                          
 */
static void   allocate_superblock()
{
    heap = (superblock *)allocate_object(&creator.sb, SBSIZE);
    if(heap != NULL){
        #ifdef STATS
            sb_stats *stat =  (sb_stats *)allocate_object(&creator.sb, SBS_SIZE);
            if(stat == NULL){
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



void* malloc(size_t size)
{
    return _malloc(size);
}

void* calloc(size_t nelem, size_t size)
{
    return _calloc(nelem, size);
}

void* realloc(void *ptr, size_t size)
{
    return _realloc(ptr, size);
}

inline void free(void *ptr)
{
    _free(ptr);
}   

int posix_memalign(void **ptr, size_t alignment, size_t size)
{
    return _posix_memalign(ptr, alignment, size);
}

void* aligned_alloc(size_t alignment, size_t size)
{
    return _aligned_alloc(alignment, size);
}

void* valloc(size_t size)
{
    return _valloc(size);
}


void* pvalloc(size_t size)
{
    return _pvalloc(size);
}

void* memalign(size_t alignment, size_t size)
{
    return _memalign(alignment, size);
}

size_t malloc_usable_size(void *ptr)
{
    if(ptr == NULL)
        return 0;
    return _msize(ptr);
}

void* reallocarray(void *ptr, size_t nelem, size_t size)
{
    return _reallocarray(ptr, nelem, size);
}

/**
 * @brief           Gathers the statistics for our current thread.
 * 
 * @param  hs       Heap stats.
 */
void local_stats(sb_stats *hs)
{
    superblock *sb = thread_local_superblock();
    #ifdef STATS
        *hs = *sb->stat;
    #endif
}

/**
 * @brief       Aggregates the statistics of superblocks.
 * 
 * @param hs    Heap stats.
 */
void global_stats(sb_stats *hs)
{
    #ifdef STATS
        init_stat(hs);
        pthread_mutex_lock(&creator.lock);
        listnode *head = &creator.heaps;
        listnode *cur  = list_first(head);
        sb_stats *stat = NULL;
        superblock *sb = NULL;
        for(;cur != NULL && cur != head; cur = cur->next){
            sb   = container_of(cur, superblock, next);
            stat = sb->stat;
            for(uint8_t i = 0; i < NUM_CACHES; ++i){
                hs->caches[i].malloc    += stat->caches[i].malloc;
                hs->caches[i].free      += stat->caches[i].free;
                hs->caches[i].remote    += stat->caches[i].remote;
                hs->caches[i].local     += stat->caches[i].local;
                hs->caches[i].frag      += stat->caches[i].frag;
                hs->caches[i].inuse     += stat->caches[i].inuse;
                hs->caches[i].capacity  += stat->caches[i].capacity;
            }
            
            hs->lactive     += stat->lactive;
            hs->linuse      += stat->linuse;
            hs->lcapacity   += stat->lcapacity;
            hs->malloc      += stat->malloc;
            hs->free        += stat->free;
            hs->remote      += stat->remote;
            hs->local       += stat->local; 
            hs->frag        += stat->frag;
            hs->inuse       += stat->inuse;
            hs->capacity    += stat->capacity;
            hs->tslabs      += stat->tslabs;
            hs->orphaned    += stat->orphaned;
            hs->requested   += stat->requested;
        }
        pthread_mutex_unlock(&creator.lock);

        
        hs->malloc +=  atomic_load_explicit(&gs.malloc, memory_order_relaxed);
        hs->free   +=  atomic_load_explicit(&gs.free, memory_order_relaxed);
        hs->remote +=  atomic_load_explicit(&gs.remote, memory_order_relaxed);
        hs->local  +=  atomic_load_explicit(&gs.local, memory_order_relaxed);
        hs->requested += atomic_load_explicit(&gs.requested, memory_order_relaxed);
        hs->released += atomic_load_explicit(&gs.released, memory_order_relaxed);
        hs->frag += atomic_load_explicit(&recycle.frag, memory_order_relaxed);
        hs->inuse += atomic_load_explicit(&recycle.inuse, memory_order_relaxed);
        hs->capacity += atomic_load_explicit(&recycle.capacity, memory_order_relaxed);
        hs->tslabs += atomic_load_explicit(&recycle.tslabs, memory_order_relaxed);
        hs->linuse += atomic_load_explicit(&recycle.linuse, memory_order_relaxed);
        hs->lactive += atomic_load_explicit(&recycle.lactive, memory_order_relaxed);
        hs->lcapacity += atomic_load_explicit(&recycle.lcapacity, memory_order_relaxed);
        for(uint8_t i = 0; i < NUM_CACHES; ++i){
            hs->caches[i].frag    += atomic_load_explicit(&recycle.caches[i].frag,
                                        memory_order_relaxed);
            hs->caches[i].inuse  += atomic_load_explicit(&recycle.caches[i].inuse, 
                                        memory_order_relaxed);
            hs->caches[i].capacity += atomic_load_explicit(&recycle.caches[i].capacity,
                                        memory_order_relaxed);
        }

        if(hs->capacity > atomic_load_explicit(&gs.peak, memory_order_relaxed)){
            hs->peak = hs->capacity;
            atomic_store_explicit(&gs.peak, hs->capacity, memory_order_relaxed);
        }else{
            hs->peak = atomic_load_explicit(&gs.peak, memory_order_relaxed);
        }


        for(uint8_t i = 0; i < NUM_CACHES; ++i){
            if(hs->caches[i].capacity > 
                atomic_load_explicit(&gs.peaks[i].peak, memory_order_relaxed)){
                hs->caches[i].peak = hs->caches[i].capacity;
                atomic_store_explicit(&gs.peaks[i].peak, hs->caches[i].peak, memory_order_relaxed);
            }else{
                hs->caches[i].peak  =  atomic_load_explicit(&gs.peaks[i].peak, 
                    memory_order_relaxed);
            }
        }
    #endif
}



void* rmalloc(size_t size){return _malloc(size);}
void* rcalloc(size_t nelem, size_t size){return _calloc(nelem, size);}
void* rrealloc(void *ptr, size_t size){return _realloc(ptr, size);}
void  rfree(void *ptr){_free(ptr);}   

int rposix_memalign(void **ptr, size_t alignment, size_t size)
{
    return _posix_memalign(ptr, alignment, size);
}

void* raligned_alloc(size_t alignment, size_t size)
{
    return _aligned_alloc(alignment, size);
}

void* rvalloc(size_t size){return _valloc(size);}
void* rpvalloc(size_t size){return _pvalloc(size);}

void* rmemalign(size_t alignment, size_t size)
{
    return _memalign(alignment, size);
}

size_t rmalloc_usable_size(void *ptr)
{
    if(ptr == NULL)
        return 0;
    return _msize(ptr);
}

void* rreallocarray(void *ptr, size_t nelem, size_t size)
{
    return _reallocarray(ptr, nelem, size);
}
#include "../include/rmalloc/arena.h"
#include <errno.h>
#include "../include/rmalloc/recycle.h"
#include "../include/rmalloc.h"
#include "../include/rmalloc/slab.h"
#include <string.h>
#include "../include/rmalloc/stats.h"
#include "../include/rmalloc/superblock.h"
#include "../include/rmalloc/internal.h"




inline uint8_t can_we_allocate(superblock *sb)
{
    extern thread_local size_t sk;
    /**
     * @brief   Only the thread that created the superblock can use it.
     *          Return early if the allocation is not from the owning 
     *          thread.
     */
    return sb->ok == sk && sb->status == ARENA;
}

inline superblock* rarena_allocate()
{
    extern thread_local size_t sk;
    extern god creator;
    superblock *sb = rmalloc(SBSIZE);
    if(unlikely(sb == NULL)) return NULL;
    #ifdef STATS
        sb_stats *stat = rmalloc(SBS_SIZE);
        if(stat == NULL){
            rfree(sb);
            return NULL;
        }
        init_superblock(sb, stat, ARENA);
    #else
        init_superblock(sb, NULL, ARENA);
    #endif
    /**
     * @brief   Set the owner key to the key of the thread that
     *          created this superblock.
     */
    sb->ok = sk;
    lock_mutex(&creator.lock);
    list_enqueue(&creator.heaps, &sb->next);
    atomic_fetch_add_explicit(&creator.active, 1, memory_order_relaxed);
    unlock_mutex(&creator.lock);
    return sb;
}


inline void* rarena_malloc(superblock *sb, size_t size)
{
    if(unlikely(can_we_allocate(sb) == 0)) return NULL;
    return allocate_object(sb, size);
}



inline void* rarena_calloc(superblock *sb, size_t nelem, size_t size)
{
    size_t prod = umul_overflow(nelem, size);
    if(likely(prod != 0)){
        uint8_t* obj = rarena_malloc(sb, prod);
        if(likely(obj != NULL))
            memset(obj, 0, prod);
        return obj;
    }
    return NULL;
}


inline void* rarena_realloc(superblock *sb, void *obj, size_t size)
{
    if(unlikely(obj == NULL))
        return rarena_malloc(sb, size);
    
    if(unlikely(size == 0)){
        rfree(obj);
        return obj;
    }
    if(unlikely(can_we_allocate(sb) == 0)) return NULL;
    return shrink_expand(sb, obj, size);
}


inline void* rarena_aligned(superblock *sb, size_t alignment, size_t size)
{
    uint8_t *obj = NULL;
    if(likely(can_we_allocate(sb) == 1 && is_power_of_two(alignment)))
        obj  = align_allocate(sb, size, alignment); 
    else errno = EINVAL;
    return obj;
}


inline void* rarena_reallocarray(
superblock *sb,
void *ptr, 
size_t nelem, 
size_t size)
{
    size_t prod = umul_overflow(nelem, size);
    if(likely(prod != 0))
        return rarena_realloc(sb, ptr, prod);
    return NULL;
}

uint8_t rarena_contains(superblock *sb, void *obj)
{
    slab *s = get_slab(obj);
    return sb == s->sb;
}


void rarena_deallocate(superblock *sb)
{
    clean_up_slabs(sb);
    #ifdef STATS
        dump_stats(sb->stat);
        deallocate_object(sb->sk, sb->stat);
    #endif
}

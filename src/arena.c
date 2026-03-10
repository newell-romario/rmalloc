#include "../include/rmalloc/arena.h"
#include <errno.h>
#include "../include/rmalloc/recycle.h"
#include "../include/rmalloc.h"
#include "../include/rmalloc/slab.h"
#include <string.h>
#include "../include/rmalloc/stats.h"
#include "../include/rmalloc/superblock.h"
#include "../include/rmalloc/util.h"


static inline void dump_empty_slabs(pool *);
static inline void dump_slabs_in_caches(pool *);
static inline void release_large_tracked_slabs(pool *);
static inline void release_large_empty_slabs(pool *);

inline uint8_t can_we_allocate(superblock *sb)
{
    extern thread_local size_t sk;
    /**
     * @brief   Only the thread that created the superblock can use it.
     *          Return early if the allocation is not from the owning 
     *          thread.
     */
    return sb->ok == sk;
}

inline superblock* rarena_allocate()
{
    extern thread_local size_t sk;
    extern god creator;
    superblock *sb = rmalloc(SBSIZE);
    if(sb == NULL) return NULL;
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
    pthread_mutex_lock(&creator.lock);
    list_enqueue(&creator.heaps, &sb->next);
    atomic_fetch_add_explicit(&creator.active, 1, memory_order_relaxed);
    pthread_mutex_unlock(&creator.lock);
    return sb;
}


inline void* rarena_malloc(superblock *sb, size_t size)
{
    if(can_we_allocate(sb) == 0) return NULL;
    return allocate_object(sb, size);
}



inline void* rarena_calloc(superblock *sb, size_t nelem, size_t size)
{
    size_t prod = unsigned_multiplication_overflow(nelem, size);
    if(prod != 0){
        uint8_t* obj = rarena_malloc(sb, prod);
        if(obj != NULL)
            memset(obj, 0, prod);
        return obj;
    }
    return NULL;
}


inline void* rarena_realloc(superblock *sb, void *obj, size_t size)
{
    if(obj == NULL)
        return rarena_malloc(sb, size);
    
    if(size == 0){
        rfree(obj);
        return obj;
    }
    if(can_we_allocate(sb) == 0) return NULL;
    return shrink_expand(sb, obj, size);
}


inline void* rarena_aligned(superblock *sb, size_t alignment, size_t size)
{
    uint8_t *obj = NULL;
    if(can_we_allocate(sb) == 1 && is_power_of_two(alignment))
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
    size_t prod = unsigned_multiplication_overflow(nelem, size);
    if(prod != 0)
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
    dump_empty_slabs(&sb->caches);
    dump_slabs_in_caches(&sb->caches);
    release_large_tracked_slabs(&sb->caches);
    release_large_empty_slabs(&sb->caches);
    #ifdef STATS
        dump_stats(sb->stat);
        deallocate_object(sb->sk, sb->stat);
    #endif
    extern god creator;
    pthread_mutex_lock(&creator.lock);
    atomic_fetch_sub_explicit(&creator.active, 1, memory_order_relaxed);
    list_remove(&sb->next);
    deallocate_object(sb->sk, (uint8_t *)sb);
    pthread_mutex_unlock(&creator.lock);
}


static inline void dump_empty_slabs(pool *p)
{
    extern bin recycle;
    listnode *head = &p->global;
    listnode *next = NULL;
    slab *s = NULL;
    for(listnode *cur = list_first(head); 
    cur != NULL && cur != head; 
    cur = next){
        next = cur->next;
        s = container_of(cur, slab, next);
        list_remove(cur);
        atomic_store_explicit(&s->status, RECYCLED, memory_order_relaxed);
        stack_slow_push(&recycle.global, &s->elem);
    }
}

static inline void dump_slabs_in_caches(pool *p)
{
    extern bin recycle;
    listnode *head[2] = {NULL};
    listnode *next = NULL;
    slab *s = NULL;
    cache * c = p->slabs;
    for(uint8_t i = 0; i < NUM_CACHES; ++i, ++c){
        s = c->hot;
        c->hot = NULL;
        if(s != NULL)
            list_enqueue(&c->partial, &s->next);
        for(uint8_t j = 0; j < 2; ++j){
            head[0] = &c->partial;
            head[1] = &c->full;
            for(listnode *cur = list_first(head[j]);
            cur != NULL && cur != head[j];
            cur = next){
                next = cur->next;
                s = container_of(cur, slab, next);
                list_remove(cur);
                atomic_store_explicit(&s->status, RECYCLED, memory_order_relaxed);
                stack_slow_push(&recycle.global, &s->elem);
            }
        }
    }
}

static inline void release_large_tracked_slabs(pool *p)
{
    pthread_mutex_lock(&p->lock);
    listnode *head = &p->tracked;
    listnode *next = NULL;
    slab *s        = NULL;
    for(listnode *cur = list_first(head);
     cur != NULL && cur != head; cur = next){
        s = container_of(cur, slab, next);
        next = cur->next;
        list_remove(cur);
        #ifdef STATS
            update_stats_on_orphaned_large_slab(s);
            update_stats_on_release(s);
        #endif
        destroy_extent(s->ext);
    }
    pthread_mutex_unlock(&p->lock);   
}

static inline void release_large_empty_slabs(pool *p)
{
    pthread_mutex_lock(&p->lock);
    listnode *head = &p->large;
    listnode *next = NULL;
    slab *s = NULL;
    uint16_t max = MSLABS;
    uint16_t count = 0;
    for(listnode *cur = list_first(head);
     cur != NULL && cur != head
     && count <= max; cur = next){
        s = container_of(cur, slab, next);
        next = cur->next;
        list_remove(cur);
        #ifdef STATS
            update_stats_on_orphaned_large_slab(s);
            update_stats_on_release(s);
        #endif
        destroy_extent(s->ext);
        ++count;
    }
    pthread_mutex_unlock(&p->lock);   
}

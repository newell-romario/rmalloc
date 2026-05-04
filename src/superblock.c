#include "../include/rmalloc/cache.h"
#include <errno.h>
#include "../include/rmalloc/extent.h"
#include "../include/rmalloc/pool.h"
#include "../include/rmalloc/recycle.h"
#include "../include/rmalloc/slab.h"
#include "../include/rmalloc/stats.h"
#include "../include/rmalloc/superblock.h"
#include <stdatomic.h>
#include <string.h>

extern god creator;
extern bin recycle;
extern _Atomic(size_t) timer;
static inline void* shrink(superblock*, slab *, void *, size_t);
static inline void* expand(superblock*, slab *, void *, size_t);
static inline uint8_t* get_object_start(slab*,  uint8_t *);
static inline void try_recycle_memory(superblock *);
static inline uint8_t* allocate_normal_object(cache *);
static inline uint8_t* allocate_large_object(superblock *, size_t );
static inline uint8_t* allocate_normal_object_fast_path(slab *);
static inline uint8_t* allocate_normal_object_slow_path(cache *);
static inline slab* get_next_large_slab(superblock *, size_t);
static inline slab* get_next_normal_slab(cache *);
static inline slab* next_normal_slab_fast_path(cache *);
static inline slab* next_normal_slab_slow_path(cache *);
static inline void move_slabs_to_global(pool *, extent  *);
static inline void return_large_object(superblock *,  slab *, size_t, uint8_t *);
static inline void   return_normal_object(superblock *, slab *, uint8_t *, uint8_t);
static inline cache* find_cache(pool *, size_t);
static inline void   move_empty_slabs_to_recycle_global(superblock *);
static inline void   orphan_partial_and_full_slabs(superblock *);
static inline slab*  get_recycled_slab(uint8_t);
static inline void   recycle_slabs(superblock *);
static inline void   local_free(superblock *, slab *, cache *,uint8_t *);
static inline void   remote_free(slab *, cache *, uint8_t *);
static inline void   cache_objects(superblock *, slab *, uint8_t *);
static inline void   dump_cache_objects(superblock *);

force_inline
static inline void recycle_slabs(superblock *sb)
{
    if(unlikely(sb->dirty == 1)){
        dump_normal_slabs_from_superblock(sb);
        dump_normal_slabs_from_bins();
        sb->dirty = 0; 
        sb->rm = 1;
    }
}

/**
 * @brief           Attempts to recycle idle memory to the operating system. 
 *                
 */
force_inline
static inline void try_recycle_memory(superblock *sb)
{
    /*  should only be recycling memory if superblock isn't an arena? 
        need to fix.
    */
    if(unlikely(sb->rm == 1)){
        /**
         * @brief Whenever rs is zero that means memory is being recycled
         *        by a background thread. We can return early.
         */
        sb->rm = 0;
        if(unlikely(sb->rs == 0)) return;
        size_t rt =  atomic_load_explicit(&timer, memory_order_relaxed);    
        release_memory_from_global();
        release_large_empty_slabs(sb);
        sb->time = rt;
    }
}



/**
 * @brief               A helper function that is used when deallocating
 *                      an object. When deallocating an object we can receive
 *                      aligned/unaligned objects. If we received an aligned
 *                      object we need to take special care to get the 
 *                      start of the object.
 * 
 * @param s             Slab.
 * @param obj           Object.
 * @return uint8_t*     Returns start of object.
 */
force_inline
static inline  uint8_t* get_object_start(slab *s, uint8_t *obj)
{
    if(likely(!s->aligned))
        return obj;
    size_t pos = (obj-s->base)/s->osize;
    return s->base + (pos*s->osize);    
}

/**
 * @brief           Attempts to shrink or expand a block of memory by size.
 * 
 * @param sb        Superblock.
 * @param obj       Object.
 * @param size      Size.
 * @return void*    Returns pointer to the new block, else NULL.
 */
void* shrink_expand(superblock *sb, void *obj, size_t size)
{
    slab *s = get_slab(obj);
    if(size > s->osize)
        return expand(sb, s, obj, size);
    return shrink(sb, s, obj, size);
}

/**
 * @brief           Attempts to expand a block of memory by size.
 * 
 * @param sb        Superblock.
 * @param s         Slab.
 * @param obj       Object.
 * @param size      Size.
 * @return void*    Returns a pointer to new block of memory, else NULL.
 */
static inline void* expand(superblock *sb, slab *s, void *obj, size_t size)
{
    void *ptr = NULL;
    ptr = allocate_object(sb, size);
    if(likely(ptr != NULL)){
        memcpy(ptr, obj, s->osize);
        deallocate_object(sb, sb->sk, obj);
    }
    return ptr;
}

/**
 * @brief           Attempts to the shrink block of memory.
 * 
 * @param sb        Superblock.
 * @param s         Slab
 * @param obj       Object.
 * @param size      Size.
 * @return void*    Returns pointer to new block, else NULL failed if we couldn't 
 *                  shrink the block.
 */
static inline void* shrink(superblock *sb, slab*s, void *obj, size_t size)
{
    void *ptr  = NULL;
    /**
     * @brief   If new size is is greater than 60% of slab object size,
     *          then we do a no op. On the other hand we try allocate 
     *          a smaller block and then free obj.
     */
    if(size >= s->osize*SHRINK_THRESHOLD)
        ptr = obj;
    else{
        ptr = allocate_object(sb, size);
        if(likely(ptr != NULL)){
            memcpy(ptr, obj, size);
            deallocate_object(sb, sb->sk, obj);
        }
    } 
    return ptr;
}


/**
 * @brief               Allocates a normal object or large object.
 * 
 * @param sb            Superblock.
 * @param osize         Object size.
 * @return uint8_t*     Returns an allocted object, else NULL.
 */
inline uint8_t*  allocate_object(superblock *sb, size_t osize)
{
    if(unlikely(osize == 0))
        osize = ALIGNMENT;
    else osize  = fast_round_up(osize, ALIGNMENT);
    if(likely(osize <= NORMAL_SLAB_SIZE)){
        cache *c = find_cache(&sb->caches, osize);
        return allocate_normal_object(c);
    }
        
    return allocate_large_object(sb, osize);
}


void   deallocate_object(superblock *sb, size_t sk, uint8_t *obj)
{
    slab *s = get_slab(obj);
    obj = get_object_start(s, obj);
    if(likely(s->osize <= NORMAL_SLAB_SIZE))
        return_normal_object(sb, s, obj, sk == s->sk);
    else return_large_object(s->sb, s, sk, obj);
}

force_inline
static inline uint8_t*  allocate_normal_object(cache *c)
{
    uint8_t *obj = (uint8_t *)fl_pop(&c->objects);
    if(obj == NULL){
        if(likely(c->hot != NULL))
            obj = allocate_normal_object_fast_path(c->hot);
    
        if(unlikely(obj == NULL))
            obj = allocate_normal_object_slow_path(c);

        #ifdef STATS
            if(__builtin_expect(obj != NULL, 1))
                update_stats_on_norm_allocation(c->hot);
        #endif
    }
    return obj;
}

force_inline
static inline uint8_t* allocate_normal_object_fast_path(slab *s)
{
    if(likely(s->local.next != NULL)){
        ++s->aobj;
        return (uint8_t *)fl_pop(&s->local);
    }
    
    if(unlikely(s->fast == 1)){
        if((s->bump + s->osize) <= (s->base + s->ssize)){
            ++s->aobj;
            uint8_t *obj = s->bump;
            s->bump += s->osize;
            return obj;
        }
        s->fast = 0;
    }

    stack *remote = stack_truncate(&s->remote);
    fl_set_next(&s->local, (fl *)remote);
    uint16_t lobj = 0;
    for(;remote != NULL; remote = remote->next)
        lobj++;

    /**
     * @brief   Sadly, we have a weird race condition where we truncate 
     *          the remote stack in the middle of a remote free.
     *          When this occurs lobj is going to be greater than robj
     *          initially because the object has been pushed onto the 
     *          stack but robj hasn't been updated yet by the remote
     *          thread. We sit in a spin loop waiting for the thread
     *          to update the robj.
     */
    uint16_t robj = 0;
    do{
        robj = atomic_load_explicit(&s->robj, memory_order_relaxed);
    }while(lobj > robj);

    atomic_fetch_sub_explicit(&s->robj, lobj, memory_order_relaxed);
    s->aobj -= lobj;
    if(likely(lobj > 0))
        ++s->aobj;

    #ifdef STATS
        update_stats_on_flushing_remote_list(s, lobj);
    #endif
    return (uint8_t *)fl_pop(&s->local);
}



/**
 * @brief               Gets the next partial or empty slab. 
 *                      Allocates an object from retreived slab.
 * 
 * @param c             Cache.
 * @return uint8_t*     Returns an allocated object, else NULL.
 */
static inline uint8_t*  allocate_normal_object_slow_path(cache *c)
{
    superblock *sb = container_of(c->pool, superblock, caches);
    recycle_slabs(sb);
    try_recycle_memory(sb);
    c->hot = get_next_normal_slab(c);
    if(likely(c->hot != NULL))
        return allocate_normal_object_fast_path(c->hot);
     
    return NULL;
}

/**
 * @brief               Allocates a large object. A large object is any object
 *                      > 64KiB.
 * 
 * @param sb            Superblock.
 * @param osize         Object size.
 * @return uint8_t*     Returns an allocated object, else NULL if we couldn't
 *                      allocate an object.
 */
static inline uint8_t* allocate_large_object(superblock *sb, size_t osize)
{
    slab *s = get_next_large_slab(sb, osize);
    if(likely(s != NULL)){
        uint8_t* obj = allocate_normal_object_fast_path(s);
        #ifdef STATS
            update_stats_on_large_allocation(s);
        #endif
        return obj;
    }
    return NULL;
}

/**
 * @brief       Returns an allocated object to the correct free list.
 * 
 * @param s     Slab.
 * @param obj   Object.
 * @param path  Path.
 */
force_inline
static inline void return_normal_object(superblock *lb, slab *s, uint8_t *obj, uint8_t path)
{
    switch(path){
        case 1:
            cache_objects(s->sb, s, obj);
        break;
        default:
            if(lb != NULL &&
                atomic_load_explicit(&s->status, memory_order_relaxed) == ACTIVE)
                cache_objects(lb, s, obj);
            else 
                remote_free(s, s->cache, obj);
        break;
    }
}

static inline void   local_free(superblock *sb, slab *s, cache *c, uint8_t *obj)
{
    fl_push(&s->local, (fl *)obj);
    --s->aobj;                
    #ifdef STATS
        update_stats_on_norm_deallocation(s);
    #endif
    /**
     * @brief   If the current slab is the hot slab we return right away.
     */
    if(c == NULL || c->hot == s) return;

    /**
     * @brief Check if we can move the slab to the correct list.
     */
    if(unlikely((s->aobj+1) == s->tobj || s->aobj == 0)){
        list_remove(&s->next);
        if(s->aobj == 0){
            pool  *p = c->pool;
            #ifdef STATS
                update_stats_on_partial_to_empty(s);
            #endif
            s->dirty = 1;
            ++sb->dslabs;
            if(sb->dslabs % MSLABS == 0)
                sb->dirty = 1;
            list_push(&p->global,  &s->next);
        }else
            list_push(&c->partial, &s->next);
        
        if(s->mtcl == 1){
            s->mtcl = 0;
            atomic_fetch_sub_explicit(&c->mtcl, 1, memory_order_relaxed);
        }
    }   
}

static inline void remote_free(slab *s, cache *c, uint8_t *obj)
{
    stack_slow_push(&s->remote, (stack *)obj);
    uint16_t robj = atomic_fetch_add_explicit(&s->robj, 1,
        memory_order_relaxed);
    
    uint8_t os = atomic_load_explicit(&s->status, 
        memory_order_acquire);
    switch(os){
        case ACTIVE:
            /**
             * @brief   If the current slab is the hot slab
             *          we return right away.
             */
            if(c == NULL ||c->hot == s) return;
            if(unlikely((robj+1) == (s->aobj))){
                /**
                 * @brief   We set the flag again in case it didn't 
                 *          get set on the first remote free. 
                 *          This can happen when we are moving the hot
                 *          slab to the full list. In the middle of a
                 *          move we get a remote free but we are still
                 *          the hot slab so we wouldn't set the
                 *          flag then but it should really be set 
                 *          because we are going on the full list. 
                 */
                if(s->mtcl == 0){
                    s->mtcl = 1;
                    atomic_fetch_add_explicit(&c->mtcl, 1, 
                        memory_order_relaxed);
                }
            }else if(robj == 0){
                /**
                 * @brief   On our first remote free we set the mtcl 
                 *          flag signaling we should move the slab 
                 *          to the correct list when we get the chance.
                 */
                if(s->mtcl == 0){
                    s->mtcl = 1;
                    atomic_fetch_add_explicit(&c->mtcl, 1, 
                        memory_order_relaxed);
                }
            }
        break;
        case ORPHAN:
            /**
             * @brief   When performing a remote free on an orphaned
             *          slab we move the slab to the correct location in
             *          the recycle bin.
             */
            if(atomic_compare_exchange_strong(&s->status, 
                &os, RECYCLED) == 1){
                /**
                 * @brief   When an orphan slab becomes empty we put it
                 *          on the global list in the recycle bin.
                 */
                if((robj+1) == (s->aobj)){
                    stack_slow_push(&recycle.global, &s->elem);
                }else{
                    /**
                     * @brief   Move the slab to the correct bin
                     *          in the recycle bin.
                     */
                    atomic_fetch_add_explicit(&recycle.caches[s->cpos].tslabs, 
                        1, memory_order_relaxed);
                    stack_slow_push(&recycle.bins[s->cpos], &s->elem); 
                }   
            }
        break;
        /**
         * @brief      When performing a remote free on a recycled slab
         *             we simply update the empty slab count for that 
         *             bin.
         */
        case RECYCLED:
            if((robj+1) == (s->aobj)){
                atomic_fetch_add_explicit(&recycle.caches[s->cpos].eslabs, 
                1, memory_order_relaxed);
            }
        break;
    }
}


force_inline
static inline void cache_objects(superblock *sb, slab *s, uint8_t *obj)
{
    pool *p = &sb->caches;
    cache *c = &p->slabs[s->cpos];
    fl_push(&c->objects, (fl *)obj);
}

/**
 * @brief           Returns a large object to the superblock.
 * 
 * @param sb        Superblock.
 * @param s         Slab.
 * @param sk        Superblock key.
 * @param obj       Object.
 */
static inline void return_large_object(
superblock *sb,
slab *s,
size_t sk,
uint8_t *obj)
{
    pool *p  = &sb->caches;
    s->dirty = 1;
    if(sk == sb->sk){
        /**
         * @brief Local free.
         * 
         */
        fl_push(&s->local, (fl *)obj);
        --s->aobj;
        ++sb->dslabs;
        if(sb->dslabs % MSLABS == 0)
            sb->dirty = 1;
        #ifdef STATS
            update_stats_on_large_deallocation(s);   
        #endif 
    }else{
        /**
         * @brief Remote free.
         */
        stack_slow_push(&s->remote, (stack *)obj);
        atomic_fetch_add_explicit(&s->robj, 1, memory_order_relaxed);
    }

    lock_mutex(&p->lock);
    decommit_memory(s->base, s->ssize, DECOMMIT_FREE);
    slab *block = NULL;
    listnode *head = &p->large;
    listnode *cur = list_first(head);
    for(;cur != NULL && cur != head; cur = cur->next){
        block = container_of(cur, slab, next);
        if(s->osize >= block->ssize)
            continue;
        break;
    }
    /*remove tracking*/
    list_remove(&s->next);
    if(cur == NULL)
        list_enqueue(head, &s->next);
    else 
        list_insert_before(cur, &s->next);
    unlock_mutex(&p->lock);
}


/**
 * @brief           Allocates an object and ensures the object is aligned to 
 *                  val.
 * 
 * @param sb        Superblock.
 * @param osize     Object size.
 * @param val       Val.
 * @return uint8_t* Returns aligned object, else NULL.
 */
uint8_t* align_allocate(superblock *sb, size_t osize, size_t val)
{
    uint8_t *obj = NULL;
    osize = uadd_overflow(osize, val-1);
    if(likely(osize != 0)){
        obj = allocate_object(sb, osize);
        if(likely(obj != NULL)){
            slab *s = get_slab(obj);
            s->aligned = 1;
            obj = (uint8_t*)round_up((size_t)obj, val);     
        }else errno = ENOMEM;
    }else errno = EOVERFLOW;
    return obj;
}

/**
 * @brief           Gets the size of the object passed.
 * 
 * @param obj       Object.
 * @return size_t   Returns the size of object.
 */
size_t allocation_size(uint8_t *obj)
{
    slab *s = get_slab(obj);
    return s->osize;
}


void init_superblock(superblock *sb, sb_stats *stat, uint8_t status)
{
    sb->status      = status;
    sb->sk          = atomic_fetch_add_explicit(&creator.counter,
                    1, memory_order_relaxed);
    sb->ok          = 0;
    sb->dirty       = 0;
    sb->rm          = 0;       
    sb->time        = 0;
    sb->dslabs      = 0;
    sb->reserved    = 0;
    sb->stat        = stat;
    sb->rs          = creator.rs;
    init_pool(&sb->caches);
    list_init(&sb->next);
    #ifdef STATS
        init_stat(sb->stat);
    #endif
}


/**
 * @brief           Gets the next allocatable slab from the cache. Whenever our
 *                  cache doesn't have an allocatable slab we grab one from the
 *                  global list in the pool. If that fails we use the 
 *                  opportunity to do some maintenance. Maintenance involves 
 *                  moving slabs to the correct list; the maintenance operation
 *                  is done for every cache. After performing maintenance we 
 *                  should be able to get an empty or partial slab. 
 *                  If the maintenance doesn't give us an empty or partial 
 *                  slab we just allocate a new extent.
 * 
 * @param c         Cache.
 * @return slab*    Returns next slab, else NULL if no slab is available.
 */
static inline slab* get_next_normal_slab(cache *c)
{
    slab *hot = c->hot;
    c->hot = NULL;
    if(likely(hot != NULL))
        list_enqueue(&c->full, &hot->next);

    hot = next_normal_slab_fast_path(c);
    if(unlikely(hot == NULL))
        hot = next_normal_slab_slow_path(c);
    return hot;
}

/**
 * @brief           Taking the fast path involves getting a slab from the
 *                  partial list stored inside the cache. If the partial 
 *                  list is emtpy, we get an empty slab from the pool.
 * 
 * @param c         Cache.
 * @return slab*    Returns slab, else NULL.
 */
static inline slab*  next_normal_slab_fast_path(cache *c)
{
    slab *hot = NULL;
    pool *p   = c->pool;
    listnode *next = list_pop(&c->partial);
    if(likely(next != NULL))
        hot = container_of(next, slab, next);
    else{ 
        next = list_pop(&p->global);
        if(likely(next != NULL)){
            superblock *sb = container_of(p, superblock, caches);
            hot = container_of(next, slab, next);
            #ifdef STATS
                uint8_t init = hot->init;
                init_slab(hot, c->osize, sb, c);
                update_stats_on_empty_normal_slab(hot, init);
            #else 
                init_slab(hot, c->osize, sb, c);
            #endif
        }
    }
    return hot;
}

/**
 * @brief           Takes the slow path.
 * 
 * @param c         Cache.
 * @return slab*    Returns slab, else NULL.
 */
static inline slab*  next_normal_slab_slow_path(cache *c)
{
    superblock *sb = container_of(c->pool, superblock, caches);  
    slab *s = get_recycled_slab(c->index);
    if(likely(s != NULL)){
        /*reset a few important fields*/
        s->cache = c;
        s->sk    = sb->sk;
        s->sb    = sb;
        atomic_store_explicit(&s->status, ACTIVE, memory_order_release);
        if(s->cached == 0)
            init_slab(s, c->osize, sb, c);
        sb->reserved += s->ssize; 
        #ifdef STATS
            update_stats_on_reused_normal_slab(s);
        #endif   
        return s; 
    }

    /**
     * @brief   Run the maintenance procedure for this cache alone.
     *          Call next_slab_fast_path again in hopes that we 
     *          recovered some slabs.
     */
    recover_slabs(c);
    s =  next_normal_slab_fast_path(c);
    if(likely(s != NULL))
        return s;
  
    /**
     * @brief Perform maintenance on all cache.
     */
    recover_all_slabs(c->pool);
    s = next_normal_slab_fast_path(c);
    if(likely(s != NULL))
        return s;
    
    /**
     * @brief   Creates a new extent and gets the first allocatable 
     *          slab from that extent.
     */
    extent *ext = create_extent(c->osize, sb->sk);
    if(likely(ext != NULL)){
        s = ext->slabs + ext->rslab;
        init_slab(s, c->osize, sb, c); 
        move_slabs_to_global(c->pool, ext);
        #ifdef STATS
            update_stats_on_new_normal_slab(s); 
        #endif
    }
    return s;
}

/**
 * @brief           Gets the next large slab from the superblock that can hold
 *                  an object of osize.
 * 
 * @param sb        Superblock.
 * @return slab*    Returns large slab, else NULL.
 */
static inline slab* get_next_large_slab(superblock * sb, size_t osize)
{
    slab *s = NULL;
    pool *p = &sb->caches;
    listnode *head = &p->large;
    lock_mutex(&p->lock);
    for(listnode *cur = list_first(head); 
    cur != NULL && cur != head; 
    cur = cur->next, s = NULL){
        s = container_of(cur, slab, next);
        if(s->ssize >= osize){
            list_remove(cur);
            break;
        }
    }
 
    if(unlikely(s == NULL)){
        extent *ext = create_extent(osize, sb->sk);
        if(likely(ext != NULL)){
            s = ext->slabs + ext->rslab;
            init_slab(s, osize, sb, NULL);
            sb->reserved += s->ssize;
            #ifdef STATS
                update_stats_on_new_large_slab(s);
            #endif
        }    
    }

    /*start tracking large slab*/
    if(likely(s != NULL))
        list_enqueue(&p->tracked, &s->next);
    
    unlock_mutex(&p->lock);
    return s;
}

/**
 * @brief       This function is called at the death of a superblock.
 *              In this function we move empty normal slabs to the 
 *              recycle bin global list. On the other hand, normal slabs that 
 *              aren't empty are orphaned, left waiting for adoption by a 
 *              superblock. Large slabs are returned instantly to the OS.
 *
 * @param sb    Superblock.
 */
void clean_up_slabs(superblock *sb)
{
    dump_cache_objects(sb);
    move_empty_slabs_to_recycle_global(sb);
    orphan_partial_and_full_slabs(sb);
    /*Releases the memory used by large allocations to the OS*/
    release_large_empty_slabs(sb);
    if(sb->status == ARENA)
        release_large_tracked_slabs(sb);
}


static inline void move_empty_slabs_to_recycle_global(superblock *sb)
{
    pool *p = &sb->caches;
    listnode *head = &p->global;
    listnode *next = NULL;
    slab *s = NULL;
    for(listnode *cur = list_first(head); 
    cur != NULL && cur != head; 
    cur = next){
        next = cur->next;
        s = container_of(cur, slab, next);
        list_remove(cur);
        sb->reserved -= s->ssize; 
        atomic_store_explicit(&s->status, RECYCLED, memory_order_release);   
        if(s->dirty == 1 || sb->status == ARENA)
            atomic_fetch_add_explicit(&timer, 1, memory_order_relaxed);
        stack_slow_push(&recycle.global, &s->elem);
    }
}
static inline void   dump_cache_objects(superblock *sb)
{
    pool *p = &sb->caches;
    cache *c = p->slabs;
    uint8_t *obj = NULL;
    fl *cur = NULL;
    slab *s = NULL;
    for(uint8_t i = 0; i < NUM_CACHES; ++i, ++c){
        cur = fl_truncate(&c->objects);;
        while(cur != NULL){
            obj = (uint8_t *)cur;
            cur = cur->next;
            s = get_slab(obj);
            if(s->sk == sb->sk)
                local_free(sb, s, c, obj);
            else remote_free(s, s->cache, obj);
        }
    }
}


/**
 * @brief       Helper function for clean_up_slabs.
 *              Moves empty slabs to the recycle bin. 
 *              Orphan non empty slabs.
 * 
 * @param sb    Superblock.
 */
static inline void orphan_partial_and_full_slabs(superblock *sb)
{
    pool *p = &sb->caches;
    listnode *head[2] = {NULL};
    listnode *cur  = NULL;
    listnode *next = NULL;
    slab *s = NULL;
    cache *c =  p->slabs;
    uint8_t status = ORPHAN;
    for(uint8_t i = 0; i < NUM_CACHES; ++i, ++c){
        head[0] = &c->full;
        head[1] = &c->partial;

        s = c->hot;
        c->hot = NULL;
        if(s != NULL)
            list_enqueue(&c->partial, &s->next);

        for(uint8_t j = 0; j < 2; ++j){
            for(cur = list_first(head[j]); 
            cur != NULL && cur != head[j];
            cur = next){ 
                next = cur->next;
                list_remove(cur);
                s = container_of(cur, slab, next);
                s->mtcl  = 0;
                sb->reserved -= s->ssize;
                atomic_store_explicit(&s->status, ORPHAN, memory_order_release);
                status = ORPHAN;
                if(atomic_compare_exchange_strong(&s->status, &status, RECYCLED) == 1){
                    if(slab_empty(s) || sb->status == ARENA){
                        s->dirty = 1;
                        stack_slow_push(&recycle.global, &s->elem); 
                        atomic_fetch_add_explicit(&timer, 1, memory_order_relaxed);
                    }
                }
            }
        }
    }
}



/**
 * @brief           Moves all the slabs in the extent to the global list except
 *                  the first slab.
 * 
 * @param p         Pool.
 * @param ext       Extent.
 */
static inline void move_slabs_to_global(pool *p, extent *ext)
{
    superblock *sb  = container_of(p, superblock, caches);
    uint16_t tslabs = atomic_load_explicit(&ext->tslabs, memory_order_relaxed);
    slab *s = ext->slabs + (ext->rslab + 1);
    for(uint16_t i = ext->rslab + 1; i < tslabs; ++i, ++s)
        list_enqueue(&p->global, &s->next);
    
    sb->reserved += (((uint8_t*)ext + ext->esize) - ext->base);
    #ifdef STATS
        update_stats_on_extent_creation(sb->stat, ext->esize, tslabs-1);
    #endif
}



static inline slab* get_recycled_slab(uint8_t no)
{
    uint8_t cached = 0;
    stack *top = stack_slow_pop(&recycle.global);
    if(top == NULL){
        top = stack_slow_pop(&recycle.bins[no]);  
        cached = 1; 
    }
        
    slab *s = NULL;
    if(top != NULL){
        s = container_of(top, slab, elem);
        s->cached = cached;
        #ifdef STATS
            update_stats_recycle(s, cached);
        #endif
        if(cached == 1){
            atomic_fetch_sub_explicit(&recycle.caches[s->cpos].tslabs, 
                1, memory_order_relaxed);
            if(s->dirty == 1)
                atomic_fetch_sub_explicit(&recycle.caches[s->cpos].eslabs, 
                    1, memory_order_relaxed);
        }     
    }
    return s;
}


static inline cache* find_cache(pool *p, size_t osize)
{   
    int8_t pos;
    if(likely(osize <= TINY_CACHES)){
        #if defined(WORD_SIZE)
            /**
             * @brief   When the word size is 8 that means the alignment value
             *          is 16 bytes. All the multiples of 16 that are <= 128
             *          are in the odd position. So we basically divide the
             *          object size by 16. The resulting value is doubled and
             *          one is added to give us an odd position index.
             * 
             *          On the other hand if the word size is 4 bytes we simple 
             *          subtract one from the object size and then divide by 8
             *          to get the index.
             */
            #if WORD_SIZE == 8
                pos = (((osize -1) >> ALIGNMENT_BITS) << 1) + 1; 
            #else 
                pos = (osize-1) >> ALIGNMENT_BITS;
            #endif
        #endif
        return &p->slabs[pos];
    }
    
    /**
     * @brief   Change at your own peril. When an object is larger than 128
     *          bytes we do a bit of bit shifting. Our size classes are stolen
     *          from jemalloc but the math to get the index is different.
     *          It may be even inefficient don't trust it.
     *          
     *          The quantum is always a power of two. The quantum is the 
     *          smallest power of 2 <= osize divided by 4. 
     *          The formula to get the corect index is  = (position of highest
     *          bit in quantum -1) * 4 + ((object size / quantum) - 5). 
     */
    int8_t nbits = msb(osize)-2;
    osize = fast_round_up(osize, (1 << nbits));
    pos = ((nbits-1)<<2) + ((int8_t)(osize >> nbits) - 5);
    return &p->slabs[pos];
}
#include "cache.h"
#include "extent.h"
#include "recycle.h"
#include "slab.h"
#include "stats.h"
#include <stdio.h>
#include "superblock.h"
#include <sys/mman.h>
#include "util.h"

extern god creator;
extern bin recycle;
extern g_stats gs;
extern _Atomic(size_t) timer;

_Atomic(size_t) nrelease; //number of released slabs
#define ESLABS .25

/**
 * @brief   Moves empty slabs from each bin in the recycle bin to the global
 *          list in the recycle bin. 
 * 
 */
void dump_normal_slabs_from_bins()
{
    stack *bins = recycle.bins;
    double ratio = 0;   
    stack temp, *top;
    size_t count; 
    size_t total;     
    slab *s = NULL;
    for(uint8_t i = 0; i < NUM_CACHES; ++i, ++bins){
        count = atomic_load_explicit(&recycle.caches[i].eslabs, 
            memory_order_relaxed);
        total = atomic_load_explicit(&recycle.caches[i].tslabs,
            memory_order_relaxed);
        if(total == 0)
            continue;
        ratio = (double)count/total;
        /**
         * @brief When less than 25% of the recycle bin is empty 
         *        slabs we continue.
         */
        if(ratio < ESLABS)
            continue;
        stack_init(&temp); 
        count = 0;
        while(((double)count/total) <= ratio && 
        (top = stack_slow_pop(bins)) != NULL){
            s = container_of(top, slab, elem);
            if(slab_empty(s)){
                stack_slow_push(&recycle.global, top);
                atomic_fetch_add_explicit(&timer, 1, memory_order_relaxed);
                ++count;
                #ifdef STATS
                    update_stats_on_recycle_bin(s);
                #endif
                continue;
            }
            stack_slow_push(&temp, top);
        }
        
        while((top = stack_slow_pop(&temp)) != NULL)
            stack_slow_push(bins, top);
        atomic_fetch_sub_explicit(&recycle.caches[i].eslabs, 
            count, memory_order_relaxed);
        atomic_fetch_sub_explicit(&recycle.caches[i].tslabs, 
            count, memory_order_relaxed);
    }
}

/**
 * @brief       Dumps the normal slabs from the superblock to the recycle bin.
 * 
 * @param sb    Superblock.
 */
void dump_normal_slabs_from_superblock(superblock *sb)
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
        if(s->dirty == 1){
            list_remove(cur);
            atomic_fetch_add_explicit(&timer, 1, memory_order_release);
            atomic_store_explicit(&s->status, RECYCLED, memory_order_relaxed);
            sb->reserved -= s->ssize;
            stack_slow_push(&recycle.global, &s->elem);
            #ifdef STATS
                update_stats_on_orphaned_normal_slab(sb->stat, s);
            #endif
        }
    }
}

/**
 * @brief Releases memory from the global list in the recycle bin.
 * 
 */
void release_memory_from_global()
{
    slab *s     = NULL; 
    stack *cur  = NULL;
    extent *ext = NULL;
    uint16_t tslabs = 0;
    uint16_t max    = MSLABS;
    uint16_t count  = 0;
    while((cur = stack_slow_pop(&recycle.global)) != NULL 
    && count <= max){
        s   = container_of(cur, slab, elem);
        ext = s->ext;
        madvise(s->base, s->ssize, MADV_DONTNEED);
        tslabs = atomic_fetch_sub_explicit(&ext->tslabs, 
            1, memory_order_relaxed);
        #ifdef STATS
            update_stats_recycle(s, 1);
            update_stats_on_release(s);
        #endif
        /**
         * @brief   Only the reserved slabs are left to be release to the 
         *          operating system. Instead we destroy the extent.
         */
        if((ext->rslab+1) == tslabs){
            #ifdef STATS
            atomic_fetch_add_explicit(&gs.released, 
                (ext->rslabs*NORMAL_SLAB_SIZE), memory_order_relaxed);
            #endif
            destroy_extent(ext);
        }
        ++count;
    }
    atomic_fetch_add_explicit(&nrelease, count, memory_order_relaxed);
}



/**
 * @brief           Returns large superblocks to the operating system.
 * 
 * @param sb        Superblock.
 */
void release_large_slabs(superblock *sb)
{
    pool *p = &sb->caches;
    pthread_mutex_lock(&p->lock);
    listnode *head = &p->large;
    listnode *next = NULL;
    slab *s      = NULL;
    uint16_t max = MSLABS;
    uint16_t count = 0;
    for(listnode *cur = list_first(head);
     cur != NULL && cur != head
     && count <= max; cur = next){
        s = container_of(cur, slab, next);
        sb->reserved -= s->ssize;
        next = cur->next;
        list_remove(cur);
        #ifdef STATS
            if(sb->status != ABANDONED)
                update_stats_on_large_slab_release(s);
            else
                update_stats_on_orphaned_large_slab(s);
            update_stats_on_release(s);
        #endif
        destroy_extent(s->ext);
        ++count;
    }
    pthread_mutex_unlock(&p->lock);   
    atomic_fetch_add_explicit(&nrelease, count, memory_order_relaxed);
}


/**
 * @brief      Releases superblock memory back to the creator.
 * 
 */
void release_superblock()
{
    pthread_mutex_lock(&creator.lock);
    listnode *head = &creator.heaps;
    listnode *next = NULL;
    superblock *sb = NULL;
    for(listnode *cur = list_first(head);
    cur != NULL && cur != head; cur = next){
        sb = container_of(cur, superblock, next);
        next = cur->next;
        if(sb->status == ABANDONED){
            if(sb->reserved == 0){
                list_remove(cur);
                #ifdef STATS
                    deallocate_object(creator.sb.sk, (uint8_t *)sb->stat);
                #endif
                deallocate_object(creator.sb.sk, (uint8_t *)sb);
                atomic_fetch_sub_explicit(&creator.active, 1, memory_order_relaxed);
                atomic_fetch_add_explicit(&nrelease, 1, memory_order_relaxed);
            }
        }
    }
    pthread_mutex_unlock(&creator.lock);
}

/**
 * @brief   Releases memory from the recycle bin and large slabs 
 *          from the superblock.
 * 
 */
void* release_memory(void *)
{
    static size_t count;
    while(1){
        atomic_init(&nrelease, 0);
        sleep(1);
        ++count;
        release_memory_from_global();
        /**
         * @brief Traverse each superblock freeing large slabs.
         */
        pthread_mutex_lock(&creator.lock);
        listnode *head = &creator.heaps;
        superblock *sb = NULL;
        for(listnode *cur = list_first(head);
        cur != NULL && cur != head; cur = cur->next){
            sb = container_of(cur, superblock, next);
            release_large_slabs(sb);
        }
        pthread_mutex_unlock(&creator.lock);
        /**
         * @brief   Traverses the list of superblocks and returns any
         *          abandoned superblock where all the allocated
         *          memory has been dumped.
         */
        release_superblock();
    }
}
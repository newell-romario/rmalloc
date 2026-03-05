#include <assert.h>
#include "../include/rmalloc/cache.h"
#include "../include/rmalloc/extent.h"
#include "../include/rmalloc/pool.h"
#include "../include/rmalloc/slab.h"
#include "../include/rmalloc/stats.h"
#include <stdio.h>
#include <stdlib.h>


static inline void  recover_partial_or_empty_slabs(cache *);
static inline void  recover_empty_slabs(cache *);


static inline void  recover_partial_or_empty_slabs(cache *c)
{
    listnode *head = &c->full;
    listnode *cur  = list_first(head);
    listnode *next = NULL;
    slab  *s = NULL;
    pool  *p = c->pool;
    superblock *sb = container_of(p, superblock, caches);
    uint16_t robj = 0;
    size_t mtcl = atomic_load_explicit(&c->mtcl, memory_order_relaxed);
    size_t count = 0;
    for(;cur != NULL && cur != head && mtcl != 0; cur = next){
        s = container_of(cur, slab, next);
        next = cur->next;
        if(s->mtcl == 0)
            continue;
        --mtcl;
        s->mtcl = 0;
        atomic_fetch_sub_explicit(&c->mtcl, 1, memory_order_relaxed);
        robj = atomic_load_explicit(&s->robj, memory_order_relaxed);    
        if(robj > 0){
            list_remove(cur);
            if(s->aobj == robj){
                s->dirty = 1;
                list_push(&p->global, cur); 
                ++count;
                ++sb->dslabs;
                if(count >= MSLABS)
                    sb->dirty = 1;
                #ifdef STATS
                update_stats_on_partial_to_empty(s);
                #endif
            }else
                list_push(&c->partial, cur);
        }
    }
}


static inline void  recover_empty_slabs(cache *c)
{
    listnode *head = &c->partial;
    listnode *cur  = list_first(head);
    listnode *next = NULL;
    slab *s = NULL;
    pool *p = c->pool;
    superblock *sb = container_of(p, superblock, caches);
    size_t mtcl = atomic_load_explicit(&c->mtcl, memory_order_relaxed);
    size_t count =  0;
    for(;cur != NULL && cur != head && mtcl != 0; cur = next){
        s = container_of(cur, slab, next);
        next = cur->next;
        if(slab_empty(s)){
            if(s->mtcl == 1){
                s->mtcl = 0;
                --mtcl;
                atomic_fetch_sub_explicit(&c->mtcl, 1, memory_order_relaxed);
            }
            ++count;
            ++sb->dslabs;
            if(count >= MSLABS)
                sb->dirty = 1;
            s->dirty = 1;
            list_remove(cur);
            list_push(&p->global, cur);
            #ifdef STATS
                update_stats_on_partial_to_empty(s);
            #endif
        }
    }
}


void recover_slabs(cache *c)
{
    recover_partial_or_empty_slabs(c);
    recover_empty_slabs(c);
}


void recover_all_slabs(pool *p)
{
    for(uint8_t i = 0; i < NUM_CACHES; ++i)
        recover_slabs(&p->slabs[i]);
}

inline void init_cache(cache *c, pool *p, size_t osize, uint8_t index)
{
    c->index    = index;
    c->osize    = osize;
    c->pool     = p;
    c->hot      = NULL;
    atomic_init(&c->mtcl, 0);
    list_init(&c->partial);
    list_init(&c->full);
}
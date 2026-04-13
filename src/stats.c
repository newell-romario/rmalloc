#include "../include/rmalloc/stats.h"
#include "../include/rmalloc/slab.h"
extern bin recycle;
extern g_stats gs;
extern uint32_t sizes[NUM_CACHES];


inline void update_stats_on_extent_creation(
sb_stats *hs,
size_t esize,
uint16_t tslabs)
{
    hs->requested += esize;
    hs->tslabs    += tslabs;
}


inline void update_stats_on_reused_normal_slab(slab *s)
{
    sb_stats *hs = s->sb->stat;
    if(s->aobj > 0){
        hs->inuse += (s->aobj * s->osize);
        hs->caches[s->cpos].inuse += (s->aobj * s->osize);
    }

    hs->caches[s->cpos].capacity += s->ssize;
    hs->caches[s->cpos].frag     += s->frag;
    if(hs->caches[s->cpos].capacity > hs->caches[s->cpos].peak)
        hs->caches[s->cpos].peak = hs->caches[s->cpos].capacity;
    
    hs->frag     += s->frag;
    hs->capacity += s->ssize;
    ++hs->tslabs;
    if(hs->capacity > hs->peak)
        hs->peak = hs->capacity;    
}


inline void update_stats_on_new_normal_slab(slab *s)
{
    sb_stats *hs = s->sb->stat;
    hs->caches[s->cpos].capacity += s->ssize;
    hs->caches[s->cpos].frag  += s->frag;
    if(hs->caches[s->cpos].capacity > hs->caches[s->cpos].peak)
        hs->caches[s->cpos].peak = hs->caches[s->cpos].capacity;
    
    hs->capacity += s->ssize;
    hs->frag     += s->frag;
    if(hs->capacity > hs->peak)
        hs->peak = hs->capacity;
}


void update_stats_on_empty_normal_slab(slab *s, uint8_t init)
{
    sb_stats *hs = s->sb->stat;
    hs->frag += s->frag;
    hs->caches[s->cpos].capacity += s->ssize;


    if(init == 0){
        hs->capacity += s->ssize;
        if(hs->capacity > hs->peak)
            hs->peak = hs->capacity;
    }

    if(hs->caches[s->cpos].capacity > hs->caches[s->cpos].peak)
        hs->caches[s->cpos].peak = hs->caches[s->cpos].capacity;
}


inline void update_stats_on_norm_allocation(slab *s)
{
 
    sb_stats *hs = s->sb->stat;
    ++hs->malloc;
    hs->inuse += s->osize;
    

    ++hs->caches[s->cpos].malloc;
    hs->caches[s->cpos].inuse += s->osize;
}

inline void update_stats_on_norm_deallocation(slab *s)
{
    /*Updates global stats*/
    sb_stats *hs = s->sb->stat;
    hs->inuse -= s->osize;
    ++hs->free;
    ++hs->local;

    /*Update cache stats*/
    ++hs->caches[s->cpos].free;
    ++hs->caches[s->cpos].local;
    hs->caches[s->cpos].inuse -= s->osize;
}



inline void update_stats_on_partial_to_empty(slab *s)
{
    /*Update global stats*/
    sb_stats *hs = s->sb->stat;
    hs->frag -= s->frag;
    
    /*Update cache stats*/
    hs->caches[s->cpos].capacity -= s->ssize;
    hs->caches[s->cpos].frag     -= s->frag;
}




inline void update_stats_on_flushing_remote_list(
slab *s, 
uint16_t robj)
{
    sb_stats *hs = s->sb->stat;

    /*Update global stats*/
    hs->free   += robj;
    hs->remote += robj;
    hs->inuse  -= (s->osize*robj);
    if(s->ssize <= NORMAL_SLAB_SIZE){
        /*Update cache stat*/
        hs->caches[s->cpos].free += robj;
        hs->caches[s->cpos].remote += robj;
        hs->caches[s->cpos].inuse -= (s->osize*robj);
    }else{
        hs->linuse -= s->osize;
        hs->frag   -= (s->ssize - s->osize);
        --hs->lactive;
    }
}




inline void update_stats_on_large_allocation(slab *s)
{
    sb_stats *hs = s->sb->stat;
    ++hs->malloc;
    hs->linuse += s->osize;
    hs->inuse  += s->osize;
    hs->frag += (s->ssize - s->osize);
    ++hs->lactive;
}



inline void update_stats_on_large_deallocation(slab *s)
{
    sb_stats *hs = s->sb->stat;
    hs->linuse -= s->osize;
    hs->inuse  -= s->osize;
    hs->frag -= (s->ssize - s->osize);
    ++hs->free;
    --hs->lactive;
}



inline void update_stats_on_new_large_slab(slab *s)
{
    sb_stats *hs = s->sb->stat;
    extent *ext = s->ext;
    hs->requested += ext->esize;
    hs->capacity  += s->ssize;
    hs->lcapacity += s->ssize;
    ++hs->tslabs;
    if(hs->capacity > hs->peak)
        hs->peak = hs->capacity;
}


inline void update_stats_on_large_slab_release(slab *s)
{
    sb_stats *hs   = s->sb->stat;
    hs->capacity  -= s->osize;
    hs->lcapacity -= s->osize;  
}


inline void dump_stats(sb_stats *hs)
{
    atomic_fetch_add_explicit(&gs.malloc, hs->malloc, memory_order_relaxed);
    atomic_fetch_add_explicit(&gs.free, hs->free, memory_order_relaxed);
    atomic_fetch_add_explicit(&gs.local, hs->local, memory_order_relaxed);
    atomic_fetch_add_explicit(&gs.remote, hs->remote, memory_order_relaxed);
    atomic_fetch_add_explicit(&gs.requested, hs->requested, memory_order_relaxed);
  
    /**
     * @brief Dump local stats to recycle bin.
     */
    atomic_fetch_add_explicit(&recycle.frag, hs->frag, memory_order_relaxed);
    atomic_fetch_add_explicit(&recycle.inuse, hs->inuse, memory_order_relaxed);
    atomic_fetch_add_explicit(&recycle.capacity, hs->capacity, memory_order_relaxed);
    atomic_fetch_add_explicit(&recycle.tslabs, hs->tslabs, memory_order_relaxed);
    atomic_fetch_add_explicit(&recycle.linuse, hs->linuse, memory_order_relaxed);
    atomic_fetch_add_explicit(&recycle.lcapacity, hs->lcapacity, memory_order_relaxed);
    atomic_fetch_add_explicit(&recycle.lactive, hs->lactive, memory_order_relaxed);

    /**
     * @brief Dump cache stats.
     */
    for(uint8_t i = 0; i < NUM_CACHES; ++i){
        atomic_fetch_add_explicit(&recycle.caches[i].frag, 
            hs->caches[i].frag, memory_order_relaxed);

        atomic_fetch_add_explicit(&recycle.caches[i].inuse, 
            hs->caches[i].inuse, memory_order_relaxed);

        atomic_fetch_add_explicit(&recycle.caches[i].capacity,
            hs->caches[i].capacity, memory_order_relaxed);
    }

    init_stat(hs);
}


inline void update_stats_on_orphaned_large_slab(slab *s)
{
    extent *ext = s->ext;
    atomic_fetch_add_explicit(&gs.released, ext->esize, memory_order_relaxed);
    atomic_fetch_sub_explicit(&recycle.inuse, s->osize, memory_order_relaxed);
    atomic_fetch_sub_explicit(&recycle.linuse, s->osize, memory_order_relaxed);
    atomic_fetch_sub_explicit(&recycle.lactive, 1, memory_order_relaxed);
    atomic_fetch_sub_explicit(&recycle.lcapacity, s->ssize, memory_order_relaxed);
    atomic_fetch_sub_explicit(&recycle.capacity, s->osize, memory_order_relaxed);
}



inline void init_stat(sb_stats *stat)
{
    for(uint8_t i = 0; i < NUM_CACHES; ++i){
        stats s = {sizes[i], 0, 0, 0, 0, 0, 0, 0, 0};
        stat->caches[i] = s;
    }
        
    stat->lactive    = 0;
    stat->linuse     = 0;
    stat->lcapacity  = 0;
    stat->malloc     = 0;
    stat->free       = 0;
    stat->remote     = 0;
    stat->local      = 0;
    stat->frag       = 0;
    stat->inuse      = 0;
    stat->capacity   = 0;
    stat->peak       = 0;
    stat->orphaned   = 0;
    stat->requested  = 0;
    stat->released   = 0;
}


inline void init_gs(g_stats *g)
{
    atomic_init(&g->malloc, 0);
    atomic_init(&g->free, 0);
    atomic_init(&g->remote, 0);
    atomic_init(&g->local, 0);
    atomic_init(&g->released, 0);
    atomic_init(&g->requested, 0);
    atomic_init(&g->peak, 0);
    for(uint8_t i = 0; i <  NUM_CACHES; ++i)
        atomic_init(&g->peaks[i].peak, 0);
    
}



inline void update_stats_recycle(slab *s, uint8_t cached)
{
    atomic_fetch_sub_explicit(&recycle.tslabs, 
        1, memory_order_relaxed);
    if(s->init == 1){
        atomic_fetch_sub_explicit(&recycle.capacity,
        s->ssize, memory_order_relaxed);
        if(cached == 1){
            atomic_fetch_sub_explicit(&recycle.inuse, 
                (s->aobj*s->osize), memory_order_relaxed);

            atomic_fetch_sub_explicit(&recycle.frag,
                s->frag, memory_order_relaxed);
            
            atomic_fetch_sub_explicit(&recycle.caches[s->cpos].inuse, 
                (s->aobj*s->osize), memory_order_relaxed);

            atomic_fetch_sub_explicit(&recycle.caches[s->cpos].frag,
                s->frag, memory_order_relaxed);
            
            atomic_fetch_sub_explicit(&recycle.caches[s->cpos].capacity,
                s->ssize, memory_order_relaxed);
        }
    }       
}


inline void update_stats_on_orphaned_normal_slab(sb_stats *hs, slab *s)
{
    if(s->init == 1)
        hs->capacity -= s->ssize;
    hs->orphaned += s->ssize;
}


inline void update_stats_on_release(slab *s)
{
    atomic_fetch_add_explicit(&gs.released, s->ssize, memory_order_relaxed);
}


void update_stats_on_recycle_bin(slab *s)
{    
    atomic_fetch_sub_explicit(&recycle.caches[s->cpos].inuse, 
        (s->aobj*s->osize), memory_order_relaxed);

    atomic_fetch_sub_explicit(&recycle.caches[s->cpos].frag,
        s->frag, memory_order_relaxed);
    
    atomic_fetch_sub_explicit(&recycle.caches[s->cpos].capacity,
        s->ssize, memory_order_relaxed);
}

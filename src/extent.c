#include "../include/rmalloc/extent.h"
#include "../include/rmalloc/internal.h"
#include <stdatomic.h>
#include "../include/rmalloc/internal.h"
#include <stdio.h>

static inline extent*   allocate_extent(size_t);
static inline void      init_extent(extent *, size_t, size_t);
static inline void      init_slabs(extent*);
static inline void      init_metadata(extent*,  size_t, size_t);
static inline size_t    extent_size(size_t);
static inline size_t    slab_size(size_t);
static inline uint16_t  total_slabs(size_t, size_t);
static inline uint32_t  metadata_size(uint16_t);
static inline size_t    total_size(size_t);


static inline size_t extent_size(size_t osize)
{
    size_t esize = DEFAULT_EXTENT_SIZE;
    if(osize > NORMAL_SLAB_SIZE)
        esize = osize;
    return esize;
}


static inline size_t slab_size(size_t osize)
{
    size_t ssize = NORMAL_SLAB_SIZE;
    if(osize > NORMAL_SLAB_SIZE)
        ssize = round_up(osize, PAGE_SIZE);
    return ssize;
}


static inline uint16_t  total_slabs(size_t esize, size_t ssize)
{
    uint16_t tslabs = esize/ssize;
    /**
     * @brief Custom extents have 2 slabs. One the slab is used 
     *        for metadata storage and other for allocations.
     */
    if(ssize > NORMAL_SLAB_SIZE)
        tslabs = 2;
    return tslabs;
}


static inline uint32_t metadata_size(uint16_t tslabs)
{
    return (uint32_t)round_up(tslabs*SSIZE+ESIZE, NORMAL_SLAB_SIZE);
}


static inline size_t total_size(size_t osize)
{
    size_t esize    = extent_size(osize);
    size_t ssize    = slab_size(osize);
    uint16_t tslabs = total_slabs(esize, ssize); 
    uint32_t mz     = metadata_size(tslabs);
    if(unlikely(ssize > NORMAL_SLAB_SIZE)){
        esize = uadd_overflow(esize, mz); 
        if(esize != 0 && esize % PAGE_SIZE != 0){
            if(unlikely(esize > (SIZE_MAX-(PAGE_SIZE-1))))
                esize = 0;
            else esize = round_up(esize, PAGE_SIZE);
        }
    }

    return esize;
}



static inline void init_metadata(extent *ext, size_t osize, size_t sk)
{
    init_extent(ext, osize, sk);
    init_slabs(ext);
}


static inline void  init_slabs(extent *ext)
{
    slab *s = ext->slabs + ext->rslab;
    uint8_t *base = ext->base;
    uint16_t tslabs = atomic_load_explicit(&ext->tslabs, memory_order_relaxed);
    for(uint16_t i = ext->rslab; i < tslabs; ++i, ++s, base += ext->ssize){
        s->base  = base;
        s->ssize = ext->ssize;
        s->ext   = ext;
    }
} 


static inline void init_extent(extent *ext, size_t osize, size_t sk)
{
    ext->esize   = total_size(osize);
    ext->ssize   = slab_size(osize);
    ext->sk      = sk;
    atomic_init(&ext->tslabs, total_slabs(ext->esize, ext->ssize));
    ext->base    = (uint8_t *)ext + metadata_size(ext->tslabs);
    ext->slabs   = (slab *)(ext+1);
    ext->rslab   = (ext->base - (uint8_t*)ext) >> MIN_SHIFT;    
}


static inline extent* allocate_extent(size_t size)
{
    void *ext = NULL;
    #if defined(UNIX)
        size_t len      = uadd_overflow(size, EXTENT_ALIGNMENT);
        uint8_t *beg    = allocate_memory(len);
        uint8_t *end    = beg + len;
        if(beg != NULL){
            ext = (uint8_t *)round_up((size_t )beg, EXTENT_ALIGNMENT);
            size_t pre = ((uint8_t *)ext) - beg;
            size_t post = end - ((uint8_t *)ext + size);
            if(likely(pre > 0))
                deallocate_memory(beg, pre);
        
            if(likely(post > 0))
                deallocate_memory((uint8_t *)ext + size, post);     
        }
    #endif
    return ext;
}


extent* create_extent(size_t osize, size_t sk)
{
    extent *ext = NULL;
    size_t size = total_size(osize);
    if(size != 0){
        if((ext = allocate_extent(size)) != NULL)
            init_metadata(ext, osize, sk);
    }
    return ext;
}


void destroy_extent(extent *ext)
{
    deallocate_memory((void *)ext, ext->esize);
}
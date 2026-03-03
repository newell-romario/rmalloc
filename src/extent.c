#include "extent.h"
#include <stdatomic.h>
#include <sys/mman.h>
#include "util.h"
#include <stdio.h>

static inline void*     allocate_memory(size_t);
static inline void      init_extent(extent *, size_t, size_t);
static inline void      init_slabs(extent*);
static inline void      init_metadata(extent*,  size_t, size_t);
static inline size_t    extent_size(size_t);
static inline size_t    slab_size(size_t);
static inline uint16_t  total_slabs(size_t, size_t);
static inline uint32_t  metadata_size(uint16_t);
static inline size_t    total_size(size_t);



/**
 * @brief               Gets the size of the extent based on the object size.
 * 
 * @param osize         Object size.
 * @return size_t       Returns the extent size.
 */
static inline size_t extent_size(size_t osize)
{
    size_t esize = EXTENT_SIZE;
    if(osize > NORMAL_SLAB_SIZE)
        esize = osize;
    return esize;
}

/**
 * @brief           Gets the size of the slab based on the object size.
 * 
 * @param osize     Object size.
 * @return size_t   Returns slabs size.
 */
static inline size_t slab_size(size_t osize)
{
    size_t ssize = NORMAL_SLAB_SIZE;
    if(osize > NORMAL_SLAB_SIZE)
        ssize = try_round_up(osize, PAGE_SIZE);
    return ssize;
}

/**
 * @brief               Gets the total number of slabs in an extent.
 * 
 * @param esize         Extent size.
 * @param ssize         Slab size.
 * @return uint8_t      Returns the total number of slabs in an extent.
 */
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

/**
 * @brief           Calculates the amount of memory needed for metadata storage.
 * 
 * @param  tslabs   Total slabs.
 * @return uint32_t Returns the amount of memory needed for metadata storage.
 */
static inline uint32_t metadata_size(uint16_t tslabs)
{
    return (uint32_t)try_round_up(tslabs*SSIZE+ESIZE, NORMAL_SLAB_SIZE);
}

/**
 * @brief           Gets the total size of the extent.
 * 
 * @param osize     Object size.
 * @return size_t   Returns the total size of the extent.
 */
static inline size_t total_size(size_t osize)
{
    size_t esize  = extent_size(osize);
    size_t ssize  = slab_size(osize);
    uint16_t tslabs = total_slabs(esize, ssize); 
    uint32_t mz     = metadata_size(tslabs);
    if(__builtin_expect(ssize > NORMAL_SLAB_SIZE, 0)){
        esize = unsigned_addition_overflow(esize, mz); 
        if(esize != 0 && esize % PAGE_SIZE != 0){
            if(__builtin_expect(esize > (SIZE_MAX-(PAGE_SIZE-1)), 0))
                esize = 0;
            else esize = try_round_up(esize, PAGE_SIZE);
        }
    }

    return esize;
}


/**
 * @brief       Initializes metadata in extent.
 * 
 * @param ext   Extent.
 * @param osize Object size.
 * @param sk    Superblock key.
 */
static inline void init_metadata(extent *ext, size_t osize, size_t sk)
{
    init_extent(ext, osize, sk);
    init_slabs(ext);
}

/**
 * @brief           Initializes slab descriptors in extent.
 * 
 * @param ext       Extent.
 */
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

/**
 * @brief           Initializes extent.
 * 
 * @param ext       Extent.
 * @param osize     Object size.
 * @param sk        Superblock key.
 */
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

/**
 * @brief           Returns a block of memory aligned to EXTENT_ALIGNMENT.
 * 
 * @param  size     Extent size.
 * @return void*    Returns pointer to the start of memory block, else NULL.
 */
static inline void* allocate_memory(size_t size)
{    
    int32_t prot   = PROT_READ|PROT_WRITE;
    int32_t flags  = MAP_ANONYMOUS|MAP_PRIVATE|MAP_ANON|MAP_NORESERVE;
    size_t len     = unsigned_addition_overflow(size, EXTENT_ALIGNMENT);
    uint8_t *beg   = mmap(NULL, len, prot, flags, -1, 0);
    uint8_t *end   = beg + len;
    void *ext = NULL;
    if(beg != MAP_FAILED){
        ext = (uint8_t *)try_round_up((size_t )beg, EXTENT_ALIGNMENT);
        size_t pre = ((uint8_t *)ext) - beg;
        size_t post = end - ((uint8_t *)ext + size);
        if(pre > 0)
            munmap(beg, pre);
    
        if(post > 0)
            munmap((uint8_t *)ext + size, post);       
    }
    return ext;
}


/**
 * @brief           Allocates memory for the extent.
 * 
 * @param osize     Object size.
 * @param sk        Superblock key.
 * @return extent*  Returns a pointer to an extent, else NULL.
 */
extent* create_extent(size_t osize, size_t sk)
{
    extent *ext = NULL;
    size_t size = total_size(osize);
    if(size != 0){
        if((ext = allocate_memory(size)) != NULL)
            init_metadata(ext, osize, sk);
    }
    return ext;
}

/**
 * @brief       Destroys an extent.
 * 
 * @param ext   Extent.
 */
void destroy_extent(extent *ext)
{
    munmap(ext, ext->esize);
}
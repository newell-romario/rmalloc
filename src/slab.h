
#ifndef _SLAB_H_
#define _SLAB_H_
#include <assert.h>
#include "types.h"
#include "extent.h"


void init_slab(slab *, size_t, superblock *, cache *);



/**
 * @brief           Gets slab object was allocated from.
 * 
 * @param obj       Object.
 * @return slab*    Slab.
 */
__attribute__((always_inline))
static inline  slab* get_slab(uint8_t *obj)
{
    extent *ext  = get_extent(obj);
    return ext->slabs + ((obj-(uint8_t*)ext) >> MIN_SHIFT);
}

/**
 * @brief           Checks if a slab is full.
 * 
 * @param s         Slab.
 * @return uint8_t  Returns 1 when slab is full, else 0.
 */
static inline  uint8_t slab_full(slab *s)
{
    return (s->aobj - atomic_load_explicit(&s->robj, memory_order_relaxed)) == s->tobj;
}

/**
 * @brief           Checks if a slab is empty.
 * 
 * @param s         Slab.
 * @return uint8_t  Returns 1 when slab is empty, else 0.
 */
static inline  uint8_t slab_empty(slab *s)
{
    return (s->aobj - atomic_load_explicit(&s->robj, memory_order_relaxed)) == 0;
}

/**
 * @brief           Checks if a slab is partially allocated.
 * 
 * @param s         Slab.
 * @return uint8_t  Returns 1 when the slab is partially allocated, else 0.
 */
static inline uint8_t slab_partial(slab *s)
{
    uint16_t aobj = (s->aobj - atomic_load_explicit(&s->robj, memory_order_acquire));
    return  aobj > 0 && aobj < s->tobj;
}


/**
 * @brief           Checks if a slab is a large slab.
 * 
 * @param s         Slab.
 * @return uint8_t  Returns 1 if the slab is a large slab, else 0.
 */
__attribute__((always_inline))
static inline uint8_t large_slab(slab *s)
{
    return s->osize > NORMAL_SLAB_SIZE;
}


static inline  uint8_t slab_valid(slab *s)
{
    extent *ext = get_extent((uint8_t*)s);
    return (uint8_t *)s > (uint8_t *)ext &&  (uint8_t *)s < ext->base;
} 
#endif
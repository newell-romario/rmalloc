
#ifndef _EXTENT_H_
#define _EXTENT_H_
#include "types.h"
#include "util.h"

/**
 * @brief           Gets the extent the object belongs to.
 * 
 * @param obj       Object.
 * @return extent*  Returns extent.
 */
__attribute__((always_inline))
static inline  extent* get_extent(uint8_t *obj)
{
    return (extent*) ((size_t)obj & ~(EXTENT_ALIGNMENT-1));
}



/**
 * @brief               Locates the first allocatable slab. 
 * 
 * @param ext           Extent.
 * @return uint16_t     Returns the index.
 */
__attribute__((always_inline))
static inline uint16_t first_slab(extent *ext)
{
    return  (ext->base - (uint8_t*)ext) >> MIN_SHIFT;
}

extent* create_extent(size_t, size_t);
void  destroy_extent(extent*);
#endif
#ifndef CONSTANTS_H_
#define CONSTANTS_H_

#define WORD_SIZE           sizeof(void*)
#define ALIGNMENT           (WORD_SIZE << 1)
#define MIN_SHIFT           16
#define EXTENT_SHIFT        (WORD_SIZE + MIN_SHIFT)
#define PAGE_SIZE           (1<<12)
#define EXTENT_ALIGNMENT    (1<<EXTENT_SHIFT)
#define DEFAULT_EXTENT_SIZE (1<<EXTENT_SHIFT)
#define NORMAL_SLAB_SIZE    (1<<MIN_SHIFT)
#define NUM_CACHES          52
#define MSLABS              (DEFAULT_EXTENT_SIZE/NORMAL_SLAB_SIZE)
#define ALARM               ((DEFAULT_EXTENT_SIZE/NORMAL_SLAB_SIZE)<<3)
#define ESLABS              .25
#endif
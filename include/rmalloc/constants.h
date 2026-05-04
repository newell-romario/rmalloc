#ifndef CONSTANTS_H_
#define CONSTANTS_H_
#if defined(__GNUC__) || defined(__clang__)
#define WORD_SIZE   __SIZEOF_POINTER__
#endif

#if defined(WORD_SIZE)
#if WORD_SIZE == 8
#define ALIGNMENT_BITS 4
#else
#define ALIGNMENT_BITS 3
#endif
#endif

#define ALIGNMENT           (WORD_SIZE << 1)
#define MIN_SHIFT           16
#define EXTENT_SHIFT        (WORD_SIZE + MIN_SHIFT)
#define PAGE_SIZE           (1<<12)
#define EXTENT_ALIGNMENT    (1<<EXTENT_SHIFT)
#define DEFAULT_EXTENT_SIZE (1<<EXTENT_SHIFT)
#define NORMAL_SLAB_SIZE    (1<<MIN_SHIFT)
#define NUM_CACHES          52
#define MSLABS              (DEFAULT_EXTENT_SIZE>>MIN_SHIFT)
#define ESLABS              .25
#define TINY_CACHES          128

#endif
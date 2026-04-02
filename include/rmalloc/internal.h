#ifndef INTERNAL_H_
#define INTERNAL_H_

#if defined(__GNUC__) || defined(__clang__)
    #define r_likely(x)  (__builtin_expect((x), true))
    #define r_unlikely(x) (__builtin_expect((x), false))
    #define r_prefetch(x, y, z) __builtin_prefetch((x), y, z)
#else
    #define r_likely(x) (x)
    #define r_unlikely(x) (x)
    #define r_prefetch(x, y, z) 
#endif

#endif
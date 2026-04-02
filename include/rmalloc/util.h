
#ifndef UTIL_H_
#define UTIL_H_
#include <stddef.h>
#include <stdint.h>
#include "internal.h"

static inline size_t round_down(size_t num, size_t val)
{
    size_t mul;
    if((val & (val-1)) == 0)
        mul = num & ~(val-1);
    else 
        mul = (num/val)*val;
    return mul;
}


__attribute__((always_inline))
static inline uint8_t is_power_of_two(size_t size)
{
    return size &&  !(size & (size -1));
}


static inline size_t unsigned_addition_overflow(size_t a, size_t b)
{
    size_t sum;
    #if defined(__GNUC__) || defined(__clang__)
        if(SIZE_MAX == UINT64_MAX){
            if(__builtin_uaddll_overflow(a, b, &sum))
                sum = 0;
        }else if(SIZE_MAX == UINT32_MAX){
            if(__builtin_uaddl_overflow(a, b, &sum))
                sum = 0;
        }
    #else
        sum = a + b;
        if(sum <= a || sum <= b)
            sum = 0;
    #endif
    return sum;
}



__attribute__((always_inline))
static inline size_t try_round_up(size_t num, size_t val)
{
    size_t mul;
    if(r_likely((val & (val-1)) == 0))
        mul = (num+val-1) & ~(val-1);   
    else
        mul = ((num + val-1)/val)*val;
    return mul;
}


static inline size_t unsigned_multiplication_overflow(size_t a, size_t b)
{
    size_t product;
    #if defined(__GNUC__) || (__clang__)
        if(SIZE_MAX == UINT64_MAX){
            if(__builtin_umulll_overflow(a, b, &product))
                product = 0;
        }else if(SIZE_MAX == UINT32_MAX){
            if(__builtin_umull_overflow(a, b, &product))
                product = 0;
        }
    #else
        product = a*b;
        if(product <= a || product <= b)
            product = 0;
    #endif
    return product;
}


__attribute__((always_inline))
static inline  size_t  unsigned_number_of_bits(size_t val)
{
    #if defined(__GNUC__) || (__clang__)
        if(SIZE_MAX == UINT64_MAX)
            return 63 - __builtin_clzll(val);
        else if(SIZE_MAX == UINT32_MAX)
            return 63 - __builtin_clzl(val);
    #else 
        /*slow unsigned number of bits*/
        uint8_t tbits = 0;
        uint8_t nbits = (sizeof(size_t)*sizeof(uint8_t)) - 1;
        for(;nbits >= 0; --nbiits){
            if((val >> nbits) & 1)
                break;
                ++tbits;
        }
        return tbits;
    #endif
}

#define  container_of(ptr, type, member) \
(type *)((uint8_t *)ptr - offsetof(type, member))

#endif
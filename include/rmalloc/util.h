
#ifndef UTIL_H_
#define UTIL_H_
#include <stddef.h>
#include <stdint.h>


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
    if(__builtin_uaddl_overflow(a, b, &sum))
        sum = 0;
    return sum;
}



__attribute__((always_inline))
static inline size_t try_round_up(size_t num, size_t val)
{
    size_t mul;
    if(__builtin_expect((val & (val-1)) == 0, 1))
        mul = (num+val-1) & ~(val-1);   
    else
        mul = ((num + val-1)/val)*val;
    return mul;
}


static inline size_t unsigned_multiplication_overflow(size_t a, size_t b)
{
    size_t product;
    if(__builtin_umull_overflow(a, b, &product))
        product = 0;
    return product;
}


__attribute__((always_inline))
static inline  size_t  unsigned_number_of_bits(size_t val)
{
    return 63 - __builtin_clzll(val);
}

#define  container_of(ptr, type, member) \
(type *)((uint8_t *)ptr - offsetof(type, member))

#endif
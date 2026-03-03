
#ifndef _UTIL_H_
#define _UTIL_H_
#include <stdint.h>





/**
 * @brief           Rounds down num to the previous multiple of val. If the num
 *                  is already a multiple of val then it returns num.
 * 
 * @param num       Number.
 * @param val       Val.
 * @return size_t Returns previous multiple.
 */
static inline size_t round_down(size_t num, size_t val)
{
    size_t mul;
    if((val & (val-1)) == 0)
        mul = num & ~(val-1);
    else 
        mul = (num/val)*val;
    return mul;
}

/**
 * @brief           Checks if size is a power of two.
 * 
 * @param size      Size.
 * @return size_t Returns 1 whenever size is a power of two, else 0.
 */
__attribute__((always_inline))
static inline uint8_t is_power_of_two(size_t size)
{
    return size &&  !(size & (size -1));
}

/**
 * @brief               Adds two numbers and checks for overflow. 
 * 
 * @param a             Operand.
 * @param b             Operand.
 * @return size_t     Returns the sum, else 0 when an overflow happenned.
 */
static inline size_t unsigned_addition_overflow(size_t a, size_t b)
{
    size_t sum;
    if(__builtin_uaddl_overflow(a, b, &sum))
        sum = 0;
    return sum;
}

/**
 * @brief               Tries to round up to the next multiple of val.
 * 
 * @param num           Number.
 * @param val           Value.
 * @return size_t     Returns next multiple, else zero if an overflow occurred.
 */
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

/**
 * @brief               Multiplies two numbers and checks for overflow.
 * 
 * @param a             Operand.
 * @param b             Operand.
 * @return size_t     Returns the product, else 0 when an overflow happenned.
 */
static inline size_t unsigned_multiplication_overflow(size_t a, size_t b)
{
    size_t product;
    if(__builtin_umull_overflow(a, b, &product))
        product = 0;
    return product;
}

/**
 * @brief           Calculates the position of the highest set bit.
 * 
 * @param val       Value.
 * @return size_t Retutns the position of the highest set bit.
 */
__attribute__((always_inline))
static inline  size_t  unsigned_number_of_bits(size_t val)
{
    return 63 - __builtin_clzll(val);
}

#define  container_of(ptr, type, member) \
(type *)((uint8_t *)ptr - offsetof(type, member))

#endif
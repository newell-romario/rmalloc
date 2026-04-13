#ifndef INTERNAL_H_
#define INTERNAL_H_
#include "constants.h"
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#if defined(__linux__) || defined(__unix__)
    #define UNIX 1
    #include <pthread.h> 
    #include <sys/mman.h>
    #include <unistd.h>
    #define DECOMMIT_FREE MADV_FREE
    #define DECOMMIT_DONTNEED MADV_DONTNEED
    #define mutex_t  pthread_mutex_t
    #define thread_t pthread_t
    #define thread_key_t    pthread_key_t
    #define thread_once_t   pthread_once_t
    #define ONCE_INIT       PTHREAD_ONCE_INIT
#endif

#if defined(__GNUC__) || defined(__clang__)
    #define likely(x)  (__builtin_expect(!!(x), true))
    #define unlikely(x) (__builtin_expect(!!(x), false))
    #define prefetch(x, y, z) __builtin_prefetch((x), y, z)
    #define force_inline __attribute__((always_inline))
#endif



#define container_of(ptr, type, member) \
    (type *)((uint8_t *)ptr - offsetof(type, member))


static inline int lock_mutex(mutex_t *mutex)
{
    #if defined(UNIX)
        return pthread_mutex_lock(mutex);
    #endif
}

static inline int unlock_mutex(mutex_t *mutex)
{
    #if defined(UNIX)
        return pthread_mutex_unlock(mutex);
    #endif
}

static inline int try_lock_mutex(mutex_t *mutex)
{
    #if defined(UNIX)
        return pthread_mutex_trylock(mutex);
    #endif
}

static inline int default_init_mutex(mutex_t *mutex)
{
    int val;
    #if defined(UNIX)
        pthread_mutexattr_t attr;
        val = pthread_mutexattr_init(&attr);
        pthread_mutex_init(mutex, &attr);
        pthread_mutexattr_destroy(&attr);
    #endif

    return val;
}

static inline int create_detachable_thread(
thread_t *t, void* (*routine)(void *), 
void *args)
{
    #if defined(UNIX)
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        return pthread_create(t, &attr, routine, args);
    #endif
}

static inline size_t round_down(size_t num, size_t val)
{
    size_t mul;
    if ((val & (val - 1)) == 0)
        mul = num & ~(val - 1);
    else
        mul = (num / val) * val;
    return mul;
}

static inline size_t round_up(size_t num, size_t val)
{
    size_t mul;
    if (likely((val & (val - 1)) == 0))
        mul = (num + val - 1) & ~(val - 1);
    else
        mul = ((num + val - 1) / val) * val;
    return mul;
}

/**
 * @brief   Multiplies two numbers.
 *          Returns the product, else 0 signaling overflow.
 */
static inline size_t umul_overflow(size_t a, size_t b)
{
    size_t product;
    #if defined(__GNUC__) || defined(__clang__)
        #if SIZE_MAX == ULONG_MAX
            #define bumo(x, y, z) __builtin_umull_overflow((x), (y), (z))
        #elif SIZE_MAX == ULLONG_MAX
            #define bumo(x, y, z) __builtin_umulll_overflow((x), (y), (z))
        #endif
        if(bumo(a, b, &product))
            product = 0;
    #endif
        return product;
}

/**
 * @brief   Adds two numbers.
 *          Returns the sum, else 0 signaling overflow.
 */
static inline size_t uadd_overflow(size_t a, size_t b)
{
    size_t sum;
    #if defined(__GNUC__) || defined(__clang__)
        #if SIZE_MAX == ULONG_MAX
            #define buao(x, y, z) __builtin_uaddl_overflow((x), (y), (z))
        #elif SIZE_MAX == ULLONG_MAX
            #define buao(x, y, z) __builtin_uaddll_overflow((x), (y), (z))
        #endif
        if(buao(a, b, &sum))
            sum = 0;
    #endif
        return sum;
}

/**
 * @brief  Finds the position of the highest 1 bit in val.
 */
static inline size_t msb(size_t val)
{
    #if defined(__GNUC__) || defined(__clang__)
        #if SIZE_MAX == ULONG_MAX
            #define bunb(x) __builtin_clzl(x)
        #elif SIZE_MAX == ULLONG_MAX
            #define bunb(x) __builtin_clzll(x)
        #endif
        return 63 - bunb(val);
    #endif
}

static inline uint8_t is_power_of_two(size_t size)
{
    return size && !(size & (size - 1));
}

/**
 * @brief   Only allocates virtual memory.
 */
static inline void* allocate_memory(size_t size)
{
    #if defined(UNIX)
        int prot  = PROT_READ|PROT_WRITE;
        int flags = MAP_ANONYMOUS|MAP_PRIVATE|MAP_ANON|MAP_NORESERVE;
        void *mem = mmap(NULL, size, prot, flags, -1, 0);
        if(mem == MAP_FAILED)
            mem = NULL;
        return mem;
    #endif  
}

static inline int deallocate_memory(void *mem, size_t size)
{
    #if defined(UNIX)
        return munmap(mem, size);
    #endif
}

static inline int decommit_memory(void *mem, size_t size, int flags)
{
    #if defined(UNIX)
        return madvise(mem, size, flags);
    #endif 
}

static inline int init_once(thread_once_t *once, void (*routine)(void))
{
    #if defined(UNIX)
        return pthread_once(once, routine);
    #endif
}

static inline int create_key(thread_key_t *key, void (*routine)(void *))
{
    #if defined(UNIX)
        return pthread_key_create(key, routine);
    #endif
}

static inline int setspecific(thread_key_t *key, void *val)
{
    #if defined(UNIX)
        return pthread_setspecific(*key, val);
    #endif
}

#endif

#ifndef TYPES_H_
#define TYPES_H_
#include "list.h"
#include <pthread.h>
#include "stack.h"
#include <stdint.h>
#include <stdatomic.h>
#include <unistd.h>



#define WORD_SIZE  sizeof(void*)
#define ALIGNMENT (WORD_SIZE << 1)
#define MIN_SHIFT  16
#define EXTENT_SHIFT    (WORD_SIZE + MIN_SHIFT)
#define page_size   (sysconf(_SC_PAGE_SIZE))
#define EXTENT_ALIGNMENT   (1<<EXTENT_SHIFT)
#define EXTENT_SIZE (1<<EXTENT_SHIFT)
#define NORMAL_SLAB_SIZE (1<<MIN_SHIFT)

#define NUM_CACHES 52
#define MSLABS (EXTENT_SIZE/NORMAL_SLAB_SIZE)
#define ALARM ((EXTENT_SIZE/NORMAL_SLAB_SIZE)<<3)

typedef struct extent extent;
typedef struct slab slab;
typedef struct cache cache;
typedef struct superblock superblock;
typedef struct pool pool;
typedef struct group group;
typedef struct god god;
typedef struct bin bin;
typedef struct stats stats;
typedef struct sb_stats sb_stats;
typedef struct g_stats g_stats;


struct stats{
size_t  osize;/*object size*/
size_t  malloc;/*total number of objects allocated*/
size_t  free;/*total number of objects freed*/
size_t  remote;/*total number of remote frees*/
size_t  local;/*total number of local frees*/
size_t  frag;/*total fragmented bytes*/
size_t  inuse;/*total bytes of memory inuse*/
size_t  capacity;/*total number of bytes we can allocate*/
size_t  peak;/*peak bytes of memory allocated*/
};


struct sb_stats{
stats     caches[NUM_CACHES];/*stats for each cache*/
size_t    lactive;/*total number of active large allocation*/
size_t    linuse;/*total number of bytes inuse for large allocations*/
size_t    lcapacity;/*total number of bytes dedicated to large allocations*/
size_t    malloc;/*total number of allocation*/
size_t    free;/*total number of free*/
size_t    remote;/*total number of remote frees*/
size_t    local;/*total number of local frees*/
size_t    frag;/*total fragmented bytes*/
size_t    inuse;/*total bytes of memory inuse*/
size_t    capacity;/*total bytes of memory reserved*/
size_t    peak;/*peak bytes of memory allocated*/
size_t    tslabs;/*total slabs*/
size_t    orphaned;/*total number of bytes orphan*/
size_t    released;/*total number of bytes released to the OS*/
size_t    requested;/*life time number of bytes requested from the OS*/
};

struct g_stats{
_Atomic(size_t) malloc;
_Atomic(size_t) free;
_Atomic(size_t) remote;
_Atomic(size_t) local;
_Atomic(size_t) released;
_Atomic(size_t) requested;
_Atomic(size_t) peak;
struct{
_Atomic(size_t) peak; 
}peaks[NUM_CACHES];
};


#define SBS_SIZE sizeof(sb_stats)


struct extent{
uint8_t*            base;/*base address*/
slab*               slabs;/*array of slab descriptors*/
size_t              esize;/*extent size*/
size_t              ssize;/*slab size*/
size_t              sk;/*superblock key*/
uint16_t            rslab;/*number of reserved slabs*/
_Atomic(uint16_t)   tslabs;/*total active slabs*/
};


#define ESIZE sizeof(extent)

/**
 * @brief   Slabs go through three states during there lifetimes which are
 *          active, orphan, and recycled. An active slab is owned by a 
 *          superblock, while orphan slab isn't owned by any superblock
 *          and a recycled slab is currently in the recycle bin waiting
 *          to be reused by a superblock.
 */
enum slab_states{
ACTIVE    = 1,
ORPHAN    = 2,
RECYCLED  = 3
};



struct slab{
uint16_t            aobj;/*total allocated objects*/
uint16_t            tobj;/*total objects in slab*/
_Atomic(uint16_t)   robj;/*total objects freed*/
uint8_t             fast;/*use bump pointer*/
uint8_t             aligned;/*slab contains aligned allocation*/
uint8_t*            base;/*pointer to the beginning of allocatable region*/
uint8_t*            bump;/*bump pointer*/
size_t              osize;/*object size*/
size_t              ssize;/*slab size*/
fl                  local;/*thread local free list*/
stack               remote;/*remote free list*/
listnode            next;/*next active slab*/
stack               elem;/*next orphaned slab*/
superblock*         sb;/*owning superblock*/
cache*              cache;/*owning cache*/
size_t              sk;/*superblock key*/
size_t              frag;/*fragmented bytes*/
extent*             ext;/*owning extent*/
uint8_t             cpos;/*cache index*/
volatile uint8_t    dirty;/*dirty flag*/
uint8_t             init;/*set to 1 when slab is initialized*/
volatile uint8_t    mtcl;/*a hint that we need to move to the correct list*/
volatile uint8_t    cached;/*cached slab in the recycle bin*/
_Atomic(uint8_t)    status;/*slab status*/
};

#define SSIZE sizeof(slab)


struct cache{
uint8_t                 index;/*cache index*/
size_t                  osize;/*object size*/
slab*                   hot;/*slab we're currently allocating from*/
listnode                partial;/*list of partial or empty slabs*/
listnode                full;/*list of paritally, empty, and full slabs*/
pool*                   pool;/*pool that owns this cache*/
_Atomic(size_t)         mtcl;/*total partial or empty slabs on the wrong list.*/
};

#define CSIZE sizeof(cache)



struct pool{
cache                   slabs[NUM_CACHES];/*cache of slabs*/
listnode                global;/*global list of small slabs*/
listnode                large;/*list of large slabs*/
listnode                tracked;/*list of large tracked slabs*/
pthread_mutex_t         lock;/*locks the large list*/
};

#define PSIZE sizeof(pool)



enum superblock_type{
GOD         = 1,
ABANDONED   = 2,
NORMAL      = 3,
ARENA       = 4
};


struct superblock{
size_t            sk;/*superblock key*/
size_t            ok;/*owning superblock key*/
pool              caches;/*size classes*/    
uint8_t           status;/*superblock status*/
listnode          next;/*next superblock*/
size_t            time;/*last time we performed maintenance*/  
size_t            dslabs;/*number of dirty slabs*/
size_t            reserved;/*total memory reserved from the operating system*/
uint8_t           dirty;/*dirt flag*/
uint8_t           rs;/*recycling strategy*/
sb_stats          *stat;/*statistics structure*/
};

#define SBSIZE sizeof(superblock)


struct god{
_Atomic(uint8_t)    init;/*init called*/
_Atomic(size_t)     active;/*active superblocks*/
uint8_t             rs;/*recycling strategy*/
pthread_t           janitor;/*worker thread that recycles memory*/
pthread_mutex_t     lock;/*lock superblock*/
superblock          sb;/*superblock*/
sb_stats            stat;/*god's superblock stat*/
listnode            heaps;/*list of superblocks*/
_Atomic(size_t)     counter;/*id given to each superblock*/
};


struct bin{
stack             bins[NUM_CACHES];/*partial or empty slabs*/
stack             global;/*empty slabs*/
_Atomic(size_t)   frag;/*total fragmented bytes*/
_Atomic(size_t)   inuse;/*total bytes of memory inuse*/
_Atomic(size_t)   capacity;/*total number of bytes we can allocate*/
_Atomic(size_t)   tslabs;/*total number of slabs*/
_Atomic(size_t)   linuse;/*total number of bytes in use by large allocation*/
_Atomic(size_t)   lactive;/*total number of active large allocation*/
_Atomic(size_t)   lcapacity;/*total number of large bytes allocated*/
struct{
_Atomic(size_t)   frag;/*total fragmented bytes*/
_Atomic(size_t)   inuse;/*total bytes of memory inuse*/
_Atomic(size_t)   capacity;/*total number of bytes we can allocate*/
_Atomic(size_t)   tslabs;/*total slabs*/
_Atomic(size_t)   eslabs;/*total empty slabs in cache*/
}caches[NUM_CACHES];
};

#endif

#ifndef _TYPES_H_
#define _TYPES_H_
#include "list.h"
#include <pthread.h>
#include "stack.h"
#include <stdint.h>
#include <stdatomic.h>
#include <unistd.h>


/**
 * @brief Computer word size.
 */
#define WORD_SIZE  sizeof(void*)

/**
 * @brief   Objects are aligned to 8 bytes on 32 bit computer or 16 bytes on
 *          64 bit computer.
 */
#define ALIGNMENT (WORD_SIZE << 1)

/**
 * @brief   The min shift value is used in calculating the extent shift,
 *          and normal slab size. 
 */
#define MIN_SHIFT  16

/**
 * @brief The extent shift is used in calculating the extent size and extent
 *        alignment.
 */
#define EXTENT_SHIFT    (WORD_SIZE + MIN_SHIFT)


/**
 * @brief   Typical OS page size.
 */
#define PAGE_SIZE   (sysconf(_SC_PAGESIZE))

/**
 * @brief   Extents are aligned on every 1MiB address on 32 bit systems while 
 *          on 64 bit systems it's aligned on every 16MiB address.
 */
#define EXTENT_ALIGNMENT   (1<<EXTENT_SHIFT)


/**
 * @brief   Extents are 1MiB on 32 bit systems or 16MiB on 64 bit systems.
 */
#define EXTENT_SIZE (1<<EXTENT_SHIFT)

/**
 * @brief Slabs are always in 64KiB in size.
 */
#define NORMAL_SLAB_SIZE (1<<MIN_SHIFT)


/**
 * @brief Number of size classes we support.
 */
#define NUM_CACHES 52

/**
 * @brief Maximum slabs an extent can have.
 * 
 */
#define MSLABS (EXTENT_SIZE/NORMAL_SLAB_SIZE)

/**
 * @brief   Our recycling procedure happens every 2048 ticks on 64 bit system 
 *          and every 512 ticks on a 32 bit system. Whenever the alarm 
 *          goes off we set the cleanup flag and then perform the cleanup 
 *          at right opportunity.
 */
#define ALARM ((EXTENT_SIZE/NORMAL_SLAB_SIZE)<<4)

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


/**
 * @brief   The stats structure keeps track of important statistics for each 
 *          cache. In the stats structure we have malloc which represents the 
 *          number of times we allocated an object, free reprsents the 
 *          number of times we freed an object, remote represents how many 
 *          remote frees occured, local represents how many local frees occured,
 *          frag represents the total number bytes fragmented, 
 *          inuse represents the total number of bytes actively being used, 
 *          capacity represents the total number of bytes that can be used and 
 *          finally peak is the maximum capacity that we have recorded so far. 
 */
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


/**
 * @brief   Superblock stats structure centralizes all the important stats
 *          for a superblock. The fields are self explanatory so no 
 *          explanation will be provided.
 */
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


/**
 * @brief   An extent is a contiguous region of memory requested from the
 *          operating system. Superblocks request extents in either sizes of 
 *          EXTENT_SIZE or a custom size. Whenever object size is greater than
 *          64KiB then the superblock will request a custom size extent to
 *          handle that allocation. Important metadata about an extent is
 *          stored extent in the descriptor below.
 */
struct extent{
uint8_t*            base;/*base address*/
slab*               slabs;/*array of slab descriptors*/
size_t              esize;/*extent size*/
size_t              ssize;/*slab size*/
size_t              sk;/*superblock key*/
uint16_t            rslab;/*number of reserved slabs*/
_Atomic(uint16_t)   tslabs;/*total active slabs*/
};

/*Size of extent descriptor*/
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


/**
 * @brief   Extents are divided into slabs. A slab is a contiguous region of
 *          memory dedicated to one object size. A slab is managed by the slab
 *          descriptor below.
 */
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
size_t              frag;/*fragmented bytes in flag*/
extent*             ext;/*owning extent*/
uint8_t             cpos;/*cache index*/
volatile uint8_t    dirty;/*dirty flag*/
uint8_t             init;/*set to 1 when slab is initialized*/
volatile uint8_t    mtcl;/*a hint that we need to move to the correct list*/
volatile uint8_t    cached;/*cached slab in the recycle bin*/
_Atomic(uint8_t)    status;/*slab status*/
};

#define SSIZE sizeof(slab)

/**
 * @brief       A cache keeps track of normal slabs and by normal slabs we mean
 *              slabs that are 64KiB in size. A slab with size greater than 
 *              64KiB isn't tracked by a cache. A cache keeps track of slabs
 *              by using two list namely a full or partial list. The partial
 *              list keeps track of partially allocated slabs, in the rare
 *              circumstance it will have empty slabs in the list. The full
 *              list primarily keeps track of full slabs. However, like the 
 *              partial list it will have partial and empty slabs on it.
 * 
 *              Why do we allow empty slabs to be on the partial list? 
 *              Well, we allow empty slabs to be on the partial list because
 *              when the slab became empty it was too expensive to move it to 
 *              the global list in the pool. The same explanation suffices 
 *              for why we allow partial and empty slabs on the full
 *              list. If the transition from full to partial was caused by 
 *              a remote free then we can't transition the slab to the correct
 *              list until a local free happens. If it becomes empty on the 
 *              full list then we wait to run on our maintenance procedure to
 *              move slabs to the correct list.
 *              
 *              That's the high level overview of the cache. Important metadata
 *              relating to the cache is managed by the cache descriptor below.
 */
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


/**
 * @brief   Slabs get pooled in either the small or large list. If the cache
 *          becomes empty it grabs a slab from the appropriate list.
 */
struct pool{
cache                   slabs[NUM_CACHES];/*cache of slabs*/
listnode                global;/*global list of small slabs*/
listnode                large;/*list of large slabs*/
pthread_mutex_t         lock;/*locks the large list*/
};

#define PSIZE sizeof(pool)


/**
 * @brief   Superblocks are of three types namely god, abandoned, and normal. 
 *          Only one superblock is of type god. A god superblock is owned 
 *          by the main thread. The god superblock main purpose is to allocate
 *          other superblocks. Whenever a superblock is of type abandoned
 *          that means its thread died and now that superblock slabs are up 
 *          for adoption by any thread that needs extra memory. Finally, 
 *          a normal superblock means the thread that owns it is currently alive
 *          and is still allocating from the superblock. 
 */
enum superblock_type{
GOD         = 1,
ABANDONED   = 2,
NORMAL      = 3,
};

/**
 * @brief   Every thread allocating memory has a thread local superblock. All
 *          allocation and deallocation go through the superblock associated
 *          with the thread.
 */
struct superblock{
size_t            sk;/*superblock key*/
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

/**
 * @brief   Haha we're egotistical so we decided to name this data structure god.
 *          This structure houses a superblock which is responsible for
 *          allocating/deallocating other superblocks.
 */
struct god{
_Atomic(uint8_t)    init;/*init called*/
_Atomic(size_t)     active;/*active superblocks*/
uint8_t             rs;/*recycling strategy*/
pthread_t           janitor;/*worker thread that recycles memory*/
pthread_mutex_t     lock;/*lock superblock*/
superblock          sb;/*superblock*/
sb_stats            stat;/*god's superblock stat*/
listnode            heaps;/*list of superblocks*/
size_t              counter;/*id given to each superblock*/
};

/**
 * @brief   Whenever a superblock is abandoned we recycle its useable slabs.
 *          The useable slabs are stored in a recycle bin for any superblock
 *          to use. 
 */
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
#include "../include/rmalloc/bin.h"
#include <errno.h>
#include "../include/rmalloc/god.h"
#include "../include/rmalloc/recycle.h"
#include <stdio.h>
#include <string.h>
#include "../include/rmalloc/superblock.h"
#include "../include/rmalloc/stats.h"
#include "../include/rmalloc/types.h"
#include "../include/rmalloc/internal.h"
#include <sys/mman.h>

static inline void abandon(void *);



extern  god creator;
extern  bin recycle;
extern  g_stats gs;
extern  pthread_key_t key;

__attribute__((constructor)) void init()
{
    create_key(&key, abandon);  
    init_god(&creator);
    init_bin(&recycle);
    init_gs(&gs);
}



/**
 * @brief       Adds an abandon superblock to the abandoned list.
 * 
 * @param ptr   Superblock.
 */
void abandon(void *ptr)
{
    superblock *sb = (superblock *)ptr;
    clean_up_slabs(sb);
    /**
     * @brief   Always set the status of the superblock after calling 
     *          clean_up_slabs function never before.
     */
    sb->status = ABANDONED;
    #ifdef STATS
        dump_stats(sb->stat);
    #endif
    atomic_fetch_sub_explicit(&creator.active, 1, memory_order_relaxed);
}

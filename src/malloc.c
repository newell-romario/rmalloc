#include "../include/rmalloc/bin.h"
#include <errno.h>
#include "../include/rmalloc/god.h"
#include "../include/rmalloc/recycle.h"
#include <stdio.h>
#include <string.h>
#include "../include/rmalloc/superblock.h"
#include "../include/rmalloc/stats.h"
#include "../include/rmalloc/types.h"
#include "../include/rmalloc/util.h"
#include <sys/mman.h>

static inline void abandon(void *);



extern  god creator;
extern  bin recycle;
extern  g_stats gs;
extern  pthread_key_t key;

__attribute__((constructor)) void init()
{
    pthread_key_create(&key, abandon);    
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
    sb->status = ABANDONED;
    clean_up_slabs(sb);
    #ifdef STATS
        dump_stats(sb->stat);
    #endif
}

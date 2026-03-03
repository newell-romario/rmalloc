#ifndef _STATS_H_
#define _STATS_H_
#include "types.h"
void init_stat(sb_stats *);
void init_gs(g_stats *);
void update_stats_on_new_normal_slab(slab *);
void update_stats_on_reused_normal_slab(slab *);
void update_stats_on_norm_allocation(slab *);
void update_stats_on_norm_deallocation(slab *);
void update_stats_on_extent_creation(sb_stats *, size_t , uint16_t);
void update_stats_on_partial_to_empty(slab *);
void update_stats_on_flushing_remote_list(slab *, uint16_t);
void update_stats_on_large_allocation(slab *);
void update_stats_on_large_deallocation(slab *);
void update_stats_on_large_slab_release(slab *);
void update_stats_on_new_large_slab(slab *);
void update_stats_on_empty_normal_slab(slab *, uint8_t);
void update_stats_recycle(slab *, uint8_t);
void update_stats_on_orphaned_normal_slab(sb_stats *, slab *);
void dump_stats(sb_stats *);
void update_stats_on_orphaned_large_slab(slab *);
void update_stats_on_release(slab *);
void update_stats_on_recycle_bin(slab *);

#endif
add_library(rmalloc SHARED  bin.c cache.c extent.c god.c  malloc.c  pool.c  slab.c superblock.c recycle.c stats.c tables.c)
set_target_properties(rmalloc PROPERTIES POSITION_INDEPENDENT_CODE ON)
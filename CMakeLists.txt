cmake_minimum_required(VERSION 3.10.0)
project(malloc VERSION 0.1.0 LANGUAGES C CXX)
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
add_compile_options(-Wextra -Wall  -Wshadow -Wpointer-arith -Wfloat-equal -Wshadow)
add_compile_options(-Wcast-align -pthread -libm -ftls-model=initial-exec)
endif()

set(rmalloc_sources src/arena.c src/bin.c src/cache.c src/extent.c src/god.c
src/malloc.c src/pool.c src/slab.c src/superblock.c src/recycle.c src/rmalloc.c 
src/stats.c src/tables.c)
add_library(rmalloc SHARED ${rmalloc_sources})
set_target_properties(rmalloc PROPERTIES POSITION_INDEPENDENT_CODE ON)

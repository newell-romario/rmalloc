#ifndef ARENA_H_
#define ARENA_H_
#include <stddef.h>
#include <stdint.h>
typedef struct superblock superblock;

superblock* rarena_allocate();
void rarena_deallocate(superblock *);
void* rarena_malloc(superblock *, size_t);
void* rarena_calloc(superblock *, size_t, size_t);
void* rarena_realloc(superblock *, void *, size_t);
void* rarena_aligned(superblock *, size_t, size_t);
void* rarena_reallocarray(superblock *, void *, size_t, size_t);
uint8_t rarnea_contains(superblock *, void *);
uint8_t can_we_allocate(superblock *);
#endif
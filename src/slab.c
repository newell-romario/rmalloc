#include "cache.h"
#include "slab.h"
#include <stdatomic.h>
#include <stdio.h>
#include <assert.h>
#include <sys/mman.h>


/**
 * @brief       Initializes a slab.
 * 
 * @param s     Slab.
 * @param osize Object size.
 * @param sb    Superblock.
 * @param c     Cache.
 */
void init_slab(slab *s, size_t osize, superblock *sb, cache *c)
{     
    s->dirty    = 0;
    s->aobj     = 0;
    s->aligned  = 0;
    s->mtcl     = 0;
    s->fast     = 1;
    s->init     = 1;
    s->cache    = c;
    s->sb       = sb;
    s->sk       = sb->sk;
    s->osize    = osize;
    s->bump     = s->base;
    s->tobj     = s->ssize/osize;
    s->frag     = s->ssize % osize;
    if(c != NULL) s->cpos = c->index;
    atomic_init(&s->robj, 0);
    atomic_init(&s->status, ACTIVE);
    fl_init(&s->local);
    list_init(&s->next);
    stack_init(&s->elem);
    stack_init(&s->remote);
}
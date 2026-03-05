
#ifndef STACK_H_
#define STACK_H_
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>


typedef struct stack stack;
struct stack{
_Atomic(stack*) next;
};

static inline size_t    stack_empty(const stack *);
static inline void      stack_fast_push(stack *, stack *);
static inline void      stack_slow_push(stack *, stack *);
static inline stack*    stack_fast_pop(stack*);
static inline stack*    stack_slow_pop(stack *);
static inline stack*    stack_top(const stack *);
static inline void      stack_init(stack *);
static inline stack*    stack_truncate(stack*);
static inline size_t    stack_prepend(stack *, stack *);
static inline void      stack_set_next(stack *, stack *);


static inline  void stack_init(stack *s)
{
    atomic_init(&s->next, NULL);
}


static inline  size_t stack_empty(const stack *s)
{
    return atomic_load_explicit(&s->next, memory_order_relaxed) == NULL;
}


__attribute__((always_inline))
static inline void  stack_fast_push(stack *s, stack *item)
{
    item->next = atomic_load_explicit(&s->next, memory_order_relaxed);
    atomic_store_explicit(&s->next, item, memory_order_relaxed);
}



__attribute__((always_inline))
static inline void  stack_slow_push(stack *s, stack *item)
{
    stack *exp = atomic_load(&s->next);
    do{
        atomic_store(&item->next, exp);
    }while(!atomic_compare_exchange_weak(&s->next, &exp, item));
}


__attribute__((always_inline))
static inline  stack* stack_slow_pop(stack *s)
{
    stack *exp  = atomic_load(&s->next);
    stack *next;
    do{
        next = NULL;
        if(__builtin_expect(exp != NULL, 1))
           next = atomic_load(&exp->next);
    }while(!atomic_compare_exchange_weak(&s->next, &exp, next));
    return exp;
}


__attribute__((always_inline))
static inline  stack* stack_fast_pop(stack *s)
{
    stack *exp = NULL;
    if(atomic_load_explicit(&s->next, memory_order_relaxed)){
        exp = atomic_load_explicit(&s->next, memory_order_relaxed);
        atomic_store_explicit(&s->next, exp->next, memory_order_relaxed);
    }
        
    return exp;
}


static inline  stack* stack_top(const stack *s)
{
    return atomic_load(&s->next);
}


static inline  stack* stack_truncate(stack *s)
{
    return atomic_exchange(&s->next, NULL);
}


static inline size_t stack_prepend(stack *src, stack *dest)
{
    size_t nelems = 0;
    if(stack_empty(dest))
        return nelems;
        
    stack *temp = stack_truncate(dest);
    stack *pos  = temp;
    for(;pos->next != NULL; pos = pos->next, ++nelems);
    
    ++nelems;
    stack *exp = atomic_load(&src->next);
    do{
        atomic_store(&pos->next, exp);
    }while(!atomic_compare_exchange_weak(&src->next, &exp, temp));
    return nelems;
}


static inline void  stack_set_next(stack *s, stack *next)
{
    atomic_exchange_explicit(&s->next, next, memory_order_release);
}



typedef struct fast_list fl;
struct fast_list{
struct fast_list  *next; 
};

static inline uint8_t fl_empty(const fl *);
static inline fl* fl_top(const fl *);
static inline void fl_push(fl *, fl *);
static inline fl* fl_pop(fl *);
static inline void fl_init(fl *);
static inline fl* fl_truncate(fl *);
static inline void fl_set_next(fl *, fl *);


static inline void fl_init(fl *list)
{
    list->next = NULL;
}

__attribute__((always_inline))
static inline uint8_t fl_empty(const fl *list)
{
    return __builtin_expect(list->next == NULL, 0);
}


static inline fl* fl_top(const fl *list)
{
    return list->next;
}


__attribute__((always_inline))
static inline void fl_push(fl *list, fl *elem)
{
    elem->next = list->next;
    list->next = elem;
}


__attribute__((always_inline))
static inline fl* fl_pop(fl *list)
{
    fl *elem = list->next;
    fl *top  = NULL;
    if(__builtin_expect(elem != NULL, 1))
        top = elem->next;
    list->next = top;
    return elem;
}


__attribute__((always_inline))
static inline fl* fl_truncate(fl *list)
{
    fl *elem = list->next;
    list->next = NULL; 
    return elem;
}


__attribute__((always_inline))
static inline void fl_set_next(fl *list, fl *top)
{
    list->next = top;
}
#endif

#ifndef _STACK_H_
#define _STACK_H_
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>



/**
 * @brief   A lock free stack implementation. N.B. This stack suffers from the 
 *          ABA problem as well but for our purposes the ABA issue is not too
 *          concerning.
 */
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

/**
 * @brief           Initializes a stack.
 * 
 * @param s         Stack.
 */
static inline  void stack_init(stack *s)
{
    atomic_init(&s->next, NULL);
}

/**
 * @brief             Checks whether stack is empty.
 * 
 * @param s           Stack.  
 * @return uint8_t    Returns 1 when stack is empty, 0 otherwise.
 */
static inline  size_t stack_empty(const stack *s)
{
    return atomic_load_explicit(&s->next, memory_order_relaxed) == NULL;
}


/**
 * @brief           Fast push operation.  Discards all atomic operations
 *                  for speed up.
 * 
 * @param s         Stack.
 * @param item      Item.
 */
__attribute__((always_inline))
static inline void  stack_fast_push(stack *s, stack *item)
{
    item->next = atomic_load_explicit(&s->next, memory_order_relaxed);
    atomic_store_explicit(&s->next, item, memory_order_relaxed);
}


/**
 * @brief               Pushes an item on the stack.
 * 
 * @param s             Stack.
 * @param item          Item.
 */
__attribute__((always_inline))
static inline void  stack_slow_push(stack *s, stack *item)
{
    stack *exp = atomic_load(&s->next);
    do{
        atomic_store(&item->next, exp);
    }while(!atomic_compare_exchange_weak(&s->next, &exp, item));
}

/**
 * @brief               Pops an item from the stack.
 * 
 * @param s             Stack.
 * @return stack*       Returns the popped item, else NULL when stack is empty.
 */
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

/**
 * @brief           Pops an item from the stack. Discards all atomic
 *                  operations for speed up.
 * 
 * @param s         Stack.
 * @return stack*   Returns the popped item, else NULL when stack is empty.
 */
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

/**
 * @brief               Gets the top of the stack.
 * 
 * @param stack         Stack.
 * @return stack*       Returns top of the stack, else NULL if stack is empty.
 */
static inline  stack* stack_top(const stack *s)
{
    return atomic_load(&s->next);
}

/**
 * @brief               Tries to set the stack to empty.
 *                      
 * 
 * @param s             Stack.
 * @return stack*       Returns old stack.
 */
static inline  stack* stack_truncate(stack *s)
{
    return atomic_exchange(&s->next, NULL);
}

/**
 * @brief           Inserts dest at the beginning of src.
 * 
 * @param src       Source.
 * @param dest      Destination.
 * @return          Returns the number of elements prepended.
 */
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

/**
 * @brief           Sets the next pointer for the stack.
 *                  Not thread safe.
 * 
 *@param  s         Stack.
 *@param  next      Next.
 */
static inline void  stack_set_next(stack *s, stack *next)
{
    atomic_exchange_explicit(&s->next, next, memory_order_release);
}

/*******************************************************************************/

/**
 * @brief Fast implementation of a list.
 */
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

/**
 * @brief           Initializes the list.
 * 
 * @param list      List.
 */
static inline void fl_init(fl *list)
{
    list->next = NULL;
}

/**
 * @brief           Checks whether the list is empty.
 * 
 * @param list      List.
 * @return uint8_t  Returns 1 when the list is empty, else 0.
 */
__attribute__((always_inline))
static inline uint8_t fl_empty(const fl *list)
{
    return __builtin_expect(list->next == NULL, 0);
}

/**
 * @brief           Gets the first element in the list.
 * 
 * @param list      List.
 * @return fl*      Returns the first element in the list.
 */
static inline fl* fl_top(const fl *list)
{
    return list->next;
}

/**
 * @brief           Pushes an element on the list.
 * 
 * @param list      List.
 * @param elem      Element.
 */
__attribute__((always_inline))
static inline void fl_push(fl *list, fl *elem)
{
    elem->next = list->next;
    list->next = elem;
}

/**
 * @brief           Pops an element from the list.
 * 
 * @param list      List.
 * @return fl*      Returns popped element, NULL if list is empty.
 */
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

/**
 * @brief           Truncates list.
 * 
 * @param list      List.
 * @return fl*      Returns truncated list.
 */
__attribute__((always_inline))
static inline fl* fl_truncate(fl *list)
{
    fl *elem = list->next;
    list->next = NULL; 
    return elem;
}

/**
 * @brief       Sets the first element of the list to top.
 * 
 * @param list  List.
 * @param top   Top.
 */
__attribute__((always_inline))
static inline void fl_set_next(fl *list, fl *top)
{
    list->next = top;
}

#endif
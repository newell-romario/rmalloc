
#ifndef _LIST_H_
#define _LIST_H_
#include <stddef.h>
#include <stdint.h>


/**
 * @brief   A circular double linked list implementation that borrows heavily
 *          from the linux circular double linked list implementation.
 */
typedef struct listnode listnode;


struct listnode{
listnode *prev;
listnode *next;
};

static inline void list_init(listnode *);
static inline void list_push(listnode *,  listnode *);
static inline listnode* list_pop(listnode *);
static inline void list_enqueue(listnode *, listnode *);
static inline listnode* list_remove_back(listnode *);
static inline uint8_t list_empty(const listnode *);
static inline void list_remove(listnode *);
static inline void list_insert_after(listnode *, listnode *);
static inline void list_insert_before(listnode *, listnode *);
static inline listnode* list_first(const listnode *);
static inline listnode* list_last(const listnode *);


/**
 * @brief           Pushes an item on the list.
 * 
 * @param list      List.
 * @param item      Item.
 */
static inline  void list_push(listnode *list, listnode *item)
{
    item->next = list->next;
    item->prev = list;
    list->next->prev = item;
    list->next = item;
}

/**
 * @brief               Pops an item from the list.
 * 
 * @param list          List.
 * @return listnode*    Returns item, otherwise NULL when list empty.
 */
static inline  listnode* list_pop(listnode *list)
{
    if(list_empty(list)) return NULL;
    
    listnode *item = list->next; 
    list->next = item->next;
    list->next->prev = list;
    return item;
}

/**
 * @brief       Adds an item to the back of the list.
 * 
 * @param list  List.
 * @param item  Item.
 */
static inline  void list_enqueue(listnode *list, listnode *item)
{
    item->next = list;
    item->prev = list->prev;
    list->prev->next = item;
    list->prev = item;
}

/**
 * @brief               Removes an item from the back of the list.
 * 
 * @param list          List.
 * @return listnode*    Returns item, else NULL when list list empty.
 */
static inline  listnode* list_remove_back(listnode *list)
{
    if(list_empty(list)) return NULL;
    listnode *item = list->prev;
    item->prev->next = list;
    list->prev = item->prev;
    return item;
}


/**
 * @brief        Initializes a list.
 * 
 * @param list   List.
 */
static inline  void list_init(listnode *list)
{
    list->next = list->prev = list;
}

/**
 * @brief           Checks whether list is empty.
 * 
 * @param list      List.
 * @return uint8_t  Returns 1 when list is empty, else 0 otherwise.
 */
__attribute__((always_inline))
static inline  uint8_t list_empty(const listnode *list)
{
    return list->next == list;
}

/**
 * @brief         Inserts item after position.
 * 
 * @param pos     Position.  
 * @param item    Item.
 */
static inline  void list_insert_after(listnode *pos, listnode *item)
{
    item->next = pos->next;
    item->prev = pos;
    pos->next->prev = item;
    pos->next = item;
}


/**
 * @brief           Inserts item before position.
 * 
 * @param pos       Position.
 * @param item      Item.
 */
static inline  void list_insert_before(listnode *pos, listnode *item)
{
    item->next = pos; 
    item->prev = pos->prev;
    pos->prev->next = item;
    pos->prev = item;
}

/**
 * @brief       Removes specified item from the list.
 * 
 * @param item  Item.
 */
__attribute__((always_inline))
static inline  void list_remove(listnode *item)
{
    item->prev->next = item->next;
    item->next->prev = item->prev;
}

/**
 * @brief               Gets the first item in the list.
 * 
 * @param list          List.
 * @return listnode*    Returns the first item, else NULL when list is empty.
 */
__attribute__((always_inline))
static inline  listnode* list_first(const listnode *list)
{
    if(list_empty(list)) return NULL;
    
    return list->next;
}

/**
 * @brief                   Gets the last item in the list.
 * 
 * @param list              List.
 * @return listnode*        Returns last item, else NULL when list is empty.
 */
static inline  listnode* list_last(const listnode *list)
{
    if(list_empty(list)) return NULL;
        
    return list->prev;
}
#endif
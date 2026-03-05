
#ifndef LIST_H_
#define LIST_H_
#include <stddef.h>
#include <stdint.h>



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



static inline  void list_push(listnode *list, listnode *item)
{
    item->next = list->next;
    item->prev = list;
    list->next->prev = item;
    list->next = item;
}


static inline  listnode* list_pop(listnode *list)
{
    if(list_empty(list)) return NULL;
    
    listnode *item = list->next; 
    list->next = item->next;
    list->next->prev = list;
    return item;
}


static inline  void list_enqueue(listnode *list, listnode *item)
{
    item->next = list;
    item->prev = list->prev;
    list->prev->next = item;
    list->prev = item;
}


static inline  listnode* list_remove_back(listnode *list)
{
    if(list_empty(list)) return NULL;
    listnode *item = list->prev;
    item->prev->next = list;
    list->prev = item->prev;
    return item;
}



static inline  void list_init(listnode *list)
{
    list->next = list->prev = list;
}


__attribute__((always_inline))
static inline  uint8_t list_empty(const listnode *list)
{
    return list->next == list;
}


static inline  void list_insert_after(listnode *pos, listnode *item)
{
    item->next = pos->next;
    item->prev = pos;
    pos->next->prev = item;
    pos->next = item;
}



static inline  void list_insert_before(listnode *pos, listnode *item)
{
    item->next = pos; 
    item->prev = pos->prev;
    pos->prev->next = item;
    pos->prev = item;
}


__attribute__((always_inline))
static inline  void list_remove(listnode *item)
{
    item->prev->next = item->next;
    item->next->prev = item->prev;
}


__attribute__((always_inline))
static inline  listnode* list_first(const listnode *list)
{
    if(list_empty(list)) return NULL;
    
    return list->next;
}


static inline  listnode* list_last(const listnode *list)
{
    if(list_empty(list)) return NULL;
        
    return list->prev;
}
#endif
#pragma once 

#define LIST_BOF (ListNode)0
#define LIST_EOF (ListNode)0


typedef void* Pointer;

typedef int (*CompareFunc)(Pointer a, Pointer b);

typedef void (*DestroyFunc)(Pointer value);


typedef struct list* List;
typedef struct list_node* ListNode;


List list_create(DestroyFunc destroy_value);


int list_size(List list);


void list_insert_next(List list, ListNode node, Pointer value);


void list_remove_next(List list, ListNode node);


Pointer list_find(List list, Pointer value, CompareFunc compare);


DestroyFunc list_set_destroy_value(List list, DestroyFunc destroy_value);

void list_destroy(List list);


ListNode list_first(List list);
ListNode list_last(List list);


ListNode list_next(List list, ListNode node);


Pointer list_node_value(List list, ListNode node);

ListNode list_find_node(List list, Pointer value, CompareFunc compare);

ListNode list_previous(List list, ListNode target);
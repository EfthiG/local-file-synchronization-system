#include <stdlib.h>
#include <assert.h>

#include "../include/LinkedList.h"
#include "../include/Queue.h"


struct queue {
	List list;
};


Queue queue_create(DestroyFunc destroy_value) {

	Queue queue = malloc(sizeof(*queue));
	queue->list = list_create(destroy_value);
	return queue;
}

int queue_size(Queue queue) {
	return list_size(queue->list);
}

Pointer queue_front(Queue queue) {
	return list_node_value(queue->list, list_first(queue->list));
}

Pointer queue_back(Queue queue) {
	return list_node_value(queue->list, list_last(queue->list));
}

void queue_insert_back(Queue queue, Pointer value) {
	list_insert_next(queue->list, list_last(queue->list), value);	
}

ListNode queue_next_node(Queue queue, ListNode node) {
    return list_next(queue->list, node);
}

void queue_print(Queue queue, PrintFunc print_value) {
    for(ListNode node = list_first(queue->list); node != LIST_EOF; node = list_next(queue->list, node)) {
        print_value(list_node_value(queue->list, node));
    }
}


void queue_remove_front(Queue queue) {
	list_remove_next(queue->list, NULL);
}

DestroyFunc queue_set_destroy_value(Queue queue, DestroyFunc destroy_value) {
	return list_set_destroy_value(queue->list, destroy_value);
}

void queue_destroy(Queue queue) {

	list_destroy(queue->list);
	free(queue);
}
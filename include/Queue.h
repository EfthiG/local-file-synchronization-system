#pragma once 


typedef void* Pointer;

typedef int (*CompareFunc)(Pointer a, Pointer b);

typedef void (*DestroyFunc)(Pointer value);

typedef void (*PrintFunc)(Pointer value);


typedef struct queue* Queue;


Queue queue_create(DestroyFunc destroy_value);


int queue_size(Queue queue);


Pointer queue_front(Queue queue);


Pointer queue_back(Queue queue);


void queue_insert_back(Queue queue, Pointer value);


void queue_remove_front(Queue queue);

void queue_print(Queue queue, PrintFunc print_value);

DestroyFunc queue_set_destroy_value(Queue queue, DestroyFunc destroy_value);


void queue_destroy(Queue queue);

#include <stdlib.h>
#include <assert.h>
#include "../include/LinkedList.h"

// Ορισμοί των δομών
struct list_node {
	ListNode next;
	Pointer value;
};

struct list {
	ListNode first;              
	ListNode last;            
	int size;
	DestroyFunc destroy_value;
};

// Δημιουργία λίστας
List list_create(DestroyFunc destroy_value) {
	List list = malloc(sizeof(*list));
	list->first = NULL;
	list->last = NULL;
	list->size = 0;
	list->destroy_value = destroy_value;
	return list;
}

// Μέγεθος λίστας
int list_size(List list) {
	return list->size;
}

// Εισαγωγή επόμενου
void list_insert_next(List list, ListNode node, Pointer value) {
	ListNode new = malloc(sizeof(*new));
	new->value = value;

	if (node == NULL) {  // Εισαγωγή στην αρχή
		new->next = list->first;
		list->first = new;
		if (list->size == 0)
			list->last = new;
	} else {
		new->next = node->next;
		node->next = new;
		if (list->last == node)
			list->last = new;
	}

	list->size++;
}

// Αφαίρεση επόμενου
void list_remove_next(List list, ListNode node) {
	ListNode removed;

	if (node == NULL) {  // Αφαίρεση από την αρχή
		removed = list->first;
		assert(removed != NULL);

		list->first = removed->next;
		if (list->last == removed)
			list->last = NULL;
	} else {
		removed = node->next;
		assert(removed != NULL);

		node->next = removed->next;
		if (list->last == removed)
			list->last = node;
	}

	if (list->destroy_value != NULL)
		list->destroy_value(removed->value);

	free(removed);
	list->size--;
}

// Εύρεση τιμής
Pointer list_find(List list, Pointer value, CompareFunc compare) {
	ListNode node = list_find_node(list, value, compare);
	return node == NULL ? NULL : node->value;
}

// Αλλαγή destroy function
DestroyFunc list_set_destroy_value(List list, DestroyFunc destroy_value) {
	DestroyFunc old = list->destroy_value;
	list->destroy_value = destroy_value;
	return old;
}

// Καταστροφή λίστας
void list_destroy(List list) {
	ListNode node = list->first;
	while (node != NULL) {
		ListNode next = node->next;

		if (list->destroy_value != NULL)
			list->destroy_value(node->value);

		free(node);
		node = next;
	}
	free(list);
}

// Πρώτος κόμβος
ListNode list_first(List list) {
	return list->first;
}

// Τελευταίος κόμβος
ListNode list_last(List list) {
	return list->last;
}

// Επόμενος κόμβος
ListNode list_next(List list, ListNode node) {
	assert(node != NULL);
	return node->next;
}

// Τιμή κόμβου
Pointer list_node_value(List list, ListNode node) {
	assert(node != NULL);
	return node->value;
}

// Εύρεση κόμβου
ListNode list_find_node(List list, Pointer value, CompareFunc compare) {
	for (ListNode node = list->first; node != NULL; node = node->next) {
		if (compare(value, node->value) == 0)
			return node;
	}
	return NULL;
}


// Needed because `list_remove_next` requires the node BEFORE the one to remove
ListNode list_previous(List list, ListNode target) {
    if (target == LIST_BOF || target == LIST_EOF)
        return LIST_BOF;

    ListNode node = list_first(list);
    ListNode prev = LIST_BOF;

    while (node != LIST_EOF) {
        if (node == target)
            return prev;
        prev = node;
        node = list_next(list, node);
    }
    return LIST_BOF;
}
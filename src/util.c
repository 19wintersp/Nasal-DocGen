#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

#define LIST_NODE_LENGTH 64

extern char* const* argv;

struct list_node {
	void* items[LIST_NODE_LENGTH];
	struct list_node* next;
	struct list_node* prev;
};

struct list {
	int length;
	struct list_node* head;
	struct list_node* tail;
};

struct list* list_new() {
	struct list_node* node = malloc(sizeof(struct list_node));
	node->next = NULL;
	node->prev = NULL;

	struct list* this = malloc(sizeof(struct list));
	this->length = 0;
	this->head = node;
	this->tail = node;

	return this;
}

void list_free(struct list* this, void (* each)(void*)) {
	struct list_node* current = this->head;
	for (int i = 0; i < this->length / LIST_NODE_LENGTH; i++) {
		for (int j = 0; j < LIST_NODE_LENGTH; j++)
			if (each != NULL)
				each(current->items[j]);

		struct list_node* old_current = current;
		current = current->next;
		free(old_current);
	}

	for (int i = 0; i < this->length % LIST_NODE_LENGTH; i++)
		if (each != NULL)
			each(current->items[i]);
	free(current);

	free(this);
}

int list_length(struct list* this) {
	return this->length;
}

static void** list_get_ptr(struct list* this, int index) {
	if (index >= this->length) return NULL;

	struct list_node* current = this->head;
	for (int i = 0; i < index / LIST_NODE_LENGTH; i++) current = current->next;

	return current->items + index % LIST_NODE_LENGTH;
}

void* list_get(struct list* this, int index) {
	void** ptr = list_get_ptr(this, index);

	if (ptr == NULL) return NULL;
	else return *ptr;
}

void* list_set(struct list* this, int index, void* value) {
	void** ptr = list_get_ptr(this, index);

	if (ptr == NULL) {
		return NULL;
	} else {
		void* old = *ptr;
		*ptr = value;
		return old;
	}
}

void list_push(struct list* this, void* value) {
	if (this->length % LIST_NODE_LENGTH == 0 && this->length > 0) {
		struct list_node* node = malloc(sizeof(struct list_node));
		node->next = NULL;
		node->prev = this->tail;

		this->tail->next = node;
		this->tail = node;
	}

	this->length += 1;
	list_set(this, this->length - 1, value);
}

void* list_pop(struct list* this) {
	if (this->length == 0) return NULL;

	void* value = list_get(this, this->length - 1);
	this->length -= 1;

	if (this->length % LIST_NODE_LENGTH == 0 && this->length > 0) {
		this->tail->prev->next = NULL;
		free(this->tail);
	}

	return value;
}

static char* vasprintf(const char* format, va_list list1) {
	va_list list2;
	va_copy(list2, list1);

	int length = vsnprintf(NULL, 0, format, list1);
	va_end(list1);

	char* buffer = malloc(length + 1);
	vsnprintf(buffer, length + 1, format, list2);
	va_end(list2);

	return buffer;
}

char* asprintf(const char* format, ...) {
	va_list list1;
	va_start(list1, format);

	return vasprintf(format, list1);
}

void perrorf(const char* format, ...) {
	va_list list1;
	va_start(list1, format);

	char* message = vasprintf(format, list1);
	int length = strlen(argv[0]) + strlen(message) + 3;

	char* buffer = malloc(length);
	snprintf(buffer, length, "%s: %s", argv[0], message);

	perror(buffer);

	free(message);
	free(buffer);
}

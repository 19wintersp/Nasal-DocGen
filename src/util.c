#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

#define MAX_ALLOC 64

struct list {
	int length, alloc, iter;
	void** items;
};

struct list* list_new() {
	struct list* this = malloc(sizeof(struct list));
	this->length = this->alloc = 0;
	this->items = NULL;

	return this;
}

void list_free(struct list* this, void (* each)(void*)) {
	if (this == NULL) return;
	if (each == NULL) each = free;

	if (this->alloc > 0) {
		for (int i = 0; i < this->length; i++) each(this->items[i]);

		free(this->items);
	} else if (this->length == 1) {
		each((void*) this->items);
	}

	free(this);
}

int list_length(struct list* this) {
	return this->length;
}

void* list_get(struct list* this, int index) {
	if (index >= this->length) return NULL;

	return this->length == 1 ? (void*) this->items : this->items[index];
}

void* list_set(struct list* this, int index, void* value) {
	if (index >= this->length) return NULL;

	void** ptr = this->length == 1 ? (void**) &this->items : this->items + index;

	void* old = *ptr;
	*ptr = value;
	return old;
}

void list_push(struct list* this, void* value) {
	this->length++;

	if (this->alloc == 0) {
		if (this->length == 1) {
			this->items = (void**) value;
		} else {
			void** items = malloc(4 * sizeof(void*));
			items[0] = (void*) this->items;
			items[1] = value;
			items[2] = NULL;

			this->alloc = 4;
			this->items = items;
		}
	} else {
		if (this->length >= this->alloc) {
			this->alloc += this->alloc < MAX_ALLOC ? this->alloc : MAX_ALLOC;
			this->items = realloc(this->items, this->alloc * sizeof(void*));
		}

		this->items[this->length - 1] = value;
		this->items[this->length] = NULL;
	}
}

void* list_pop(struct list* this) {
	if (this->length == 0) return NULL;

	this->length--;

	if (this->alloc == 0) {
		void* value = (void*) this->items;
		this->items = NULL;
		return value;
	} else {
		void* value = this->items[this->length];
		this->items[this->length] = NULL;
		return value;
	}
}

void* list_iter(struct list* this, void* (* each)(void*, void*), void* user) {
	if (this->alloc > 0) {
		for (int i = 0; i < this->length; i++) {
			void* ret = each(this->items[i], user);
			if (ret) return ret;
		}
	} else if (this->length == 1) {
		return each((void*) this->items, user);
	}

	return NULL;
}

void list_sort(struct list* this, int (* comp)(const void*, const void*)) {
	if (this->alloc > 0 && this->length > 1)
		qsort(this->items, this->length, sizeof(void*), comp);
}

void* list_iter_start(struct list* this) {
	this->iter = 0;

	return this->alloc > 0 ? this->items[0] : (void*) this->items;
}

void* list_iter_next(struct list* this) {
	if (this->alloc == 0 || this->length <= 1) return NULL;

	this->iter++;

	if (this->iter >= this->length) return NULL;
	else return this->items[this->iter];
}

bool list_iter_continue(struct list* this) {
	return this->iter < this->length;
}

extern char* const* argv;

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

char* astrndup(const char* src, size_t length) {
	char* ret = malloc(length + 1);
	ret[length] = 0;

	strncpy(ret, src, length);

	return ret;
}

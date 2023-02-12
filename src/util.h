#ifndef UTIL_H
#define UTIL_H

#include <stdbool.h>
#include <stddef.h>

#define LIST_ITER_T(list, item, type) \
	for ( \
		type item = list_iter_start(list); \
		list_iter_continue(list); \
		item = list_iter_next(list) \
	)
#define LIST_ITER(list, item) LIST_ITER_T(list, item, void*)

struct list;

struct list* list_new();
void list_free(struct list* this, void (* each)(void*));
int list_length(struct list* this);
void* list_get(struct list* this, int index);
void* list_set(struct list* this, int index, void* value);
void list_push(struct list* this, void* value);
void* list_pop(struct list* this);
void* list_iter(struct list* this, void* (* each)(void*, void*), void* user);
void list_sort(struct list* this, int (* comp)(const void*, const void*));

void* list_iter_start(struct list* this);
bool list_iter_continue(struct list* this);
void* list_iter_next(struct list* this);

char* asprintf(const char* format, ...);
void perrorf(const char* format, ...);

char* astrndup(const char* src, size_t length);

char* read_file(const char* filename);

#endif // ifndef UTIL_H

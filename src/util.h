#ifndef UTIL_H
#define UTIL_H

struct list;

struct list* list_new();
void list_free(struct list* this, void (* each)(void*));
int list_length(struct list* this);
void* list_get(struct list* this, int index);
void* list_set(struct list* this, int index, void* value);
void list_push(struct list* this, void* value);
void* list_pop(struct list* this);

char* asprintf(const char* format, ...);
void perrorf(const char* format, ...);

#endif // ifndef UTIL_H

#ifndef PARSE_H
#define PARSE_H

#include <stdbool.h>

#include "util.h"

enum type_class {
	TYPE_NIL,
	TYPE_ANY,
	TYPE_NUM,
	TYPE_STR,
	TYPE_LIST,
	TYPE_HASH,
	TYPE_OBJ,
	TYPE_FUNC,
};

struct bare_func {
	struct list* param_typesets;
	struct list* return_typeset;
};

union type_data {
	struct list* typeset;
	char* class;
	struct bare_func func;
};

struct type {
	enum type_class type;
	union type_data data;
};

struct param {
	char* name;
	bool variable, optional;
};

enum item_type {
	ITEM_VAR,
	ITEM_FUNC,
	ITEM_CLASS,
};

struct item {
	char* filename;
	int line;
	char* name;
	char* desc;
	enum item_type type;
	struct list* items; /* param|item */
};

struct module {
	char* filename;
	int line;
	char* name;
	char* desc;
	struct list* children; /* module */
	struct list* items;    /* item */
};

int parse_file(const char* filename, struct module* module);

#endif // ifndef PARSE_H

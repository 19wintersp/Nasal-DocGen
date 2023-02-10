#ifndef MARKER_H
#define MARKER_H

#include <stdbool.h>

#include "parse.h"
#include "util.h"

struct marker_pair {
	char* desc;
	struct list* typeset;
};

struct marker_pair_named {
	char* name;
	char* desc;
	struct list* typeset;
};

struct markers {
	bool f_readonly : 1, f_var : 1, f_module : 1, f_class : 1;
	bool f_public : 1, f_private : 1, f_constructor : 1, f_static : 1;
	struct list* type;        /* type */
	struct list* returns;     /* marker_pair */
	struct list* params;      /* marker_pair_named */
	struct list* props;       /* marker_pair_named */
	struct list* inheritance; /* char* */
};

struct markers* markers_new();
void markers_free(struct markers* markers);

void parse_marker(const char* line, int length, struct markers* markers);

#endif // ifndef MARKER_H

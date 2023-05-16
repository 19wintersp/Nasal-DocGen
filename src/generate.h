#ifndef GENERATE_H
#define GENERATE_H

#include <stdbool.h>

#include "parse.h"

struct generate_options {
	const char* library;
	const char* output;
	const char* template;
	bool no_markdown;
};

struct source {
	const char* file;
	const char* alias;
};

int generate_docs(
	struct module* root,
	struct source sources[],
	struct generate_options opts
);

#endif // ifndef GENERATE_H

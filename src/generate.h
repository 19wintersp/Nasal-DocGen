#ifndef GENERATE_H
#define GENERATE_H

#include "parse.h"

struct generate_options {
	const char* output;
	const char* template;
};

int generate_docs(struct module* root, struct generate_options opts);

#endif // ifndef GENERATE_H

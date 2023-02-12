#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "generate.h"
#include "parse.h"
#include "util.h"

struct templates {
	char* item;
	char* list;
	char* module;
	char* source;
};

struct ctx {
	DIR* output;
	struct templates templates;
};

static int document_module(struct ctx ctx, struct module* module);
static int document_item(struct ctx ctx, struct item* item);
static char* default_template();
static char* load_template(const char* dir, const char* name);

int generate_docs(
	struct module* root,
	struct source sources[],
	struct generate_options opts
) {
	const char* template = opts.template;
	if (template == NULL) template = default_template();

	if (template == NULL) {
		perrorf("failed to find template directory");
		return 3;
	}

	if (mkdir(opts.output, 0) == -1) {
		if (errno != EEXIST) {
			perrorf("failed to create output dir");
			return 2;
		}
	}

	struct ctx ctx = {
		.output = opendir(opts.output),
		.templates = {
			.item = load_template(template, "item"),
			.list = load_template(template, "list"),
			.module = load_template(template, "module"),
			.source = load_template(template, "source"),
		},
	};

	if (
		!ctx.templates.item ||
		!ctx.templates.list ||
		!ctx.templates.module ||
		!ctx.templates.source
	) return 2;

	//int ret = document_module(ctx, root);
	//if (ret > 0) return ret;

	closedir(ctx.output);
	free(ctx.templates.item);
	free(ctx.templates.list);
	free(ctx.templates.module);
	free(ctx.templates.source);

	if (opts.template == NULL) free((char*) template);

	return 0;
}

static bool check_template(const char* path) {
	DIR* dir = opendir(path);
	if (!dir) return false;
	closedir(dir);
	return true;
}

static char* default_template() {
	const char* dirs;
	if ((dirs = getenv("XDG_DATA_DIRS"))) {
		do {
			int length = strcspn(dirs, ":");

			char* test = asprintf("%.*s/" NAME "/template", length, dirs);
			if (check_template(test)) return test;
			free(test);

			dirs += length + 1;
		} while (dirs[-1]);
	}

	if (check_template("/usr/share/" NAME "/template"))
		return "/usr/share/" NAME "/template";
	if (check_template("/usr/share/local/" NAME "/template"))
		return "/usr/share/local/" NAME "/template";

	return NULL;
}

static char* load_template(const char* dir, const char* name) {
	char* filename = asprintf("%s/pages/%s.html", dir, name);
	char* contents = read_file(filename);
	free(filename);

	puts(name);
	puts(contents);

	return contents;
}

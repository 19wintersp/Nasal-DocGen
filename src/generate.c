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

static int document_module(DIR* dir, struct module* module);
static int document_item(DIR* dir, struct item* item);
static char* default_template();

int generate_docs(struct module* root, struct generate_options opts) {
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

	DIR* dir = opendir(opts.output);

	//int ret = document_module(dir, root);
	//if (ret > 0) return ret;

	closedir(dir);

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

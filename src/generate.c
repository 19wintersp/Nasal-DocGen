#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cmark.h>

#include <cjson/cJSON.h>

#include <lattice/lattice-cjson.h>

#include "generate.h"
#include "parse.h"
#include "util.h"

#define DIR_FLAGS  (S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)
#define FILE_FLAGS (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)

struct templates {
	const char *dir;
	char* item;
	char* list;
	char* module;
	char* source;
};

struct ctx {
	DIR* output;
	struct templates templates;
	struct list* stack;
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

	if (mkdir(opts.output, DIR_FLAGS) == -1) {
		if (errno != EEXIST) {
			perrorf("failed to create output dir");
			return 2;
		}
	}

	struct ctx ctx = {
		.output = opendir(opts.output),
		.templates = {
			.dir = template,
			.item = load_template(template, "item"),
			.list = load_template(template, "list"),
			.module = load_template(template, "module"),
			.source = load_template(template, "source"),
		},
		.stack = list_new(),
	};

	if (
		!ctx.templates.item ||
		!ctx.templates.list ||
		!ctx.templates.module ||
		!ctx.templates.source
	) return 2;

	int ret = document_module(ctx, root);
	if (ret > 0) return ret;

	closedir(ctx.output);
	free(ctx.templates.item);
	free(ctx.templates.list);
	free(ctx.templates.module);
	free(ctx.templates.source);
	list_free(ctx.stack, NULL);

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

	return contents;
}

static cJSON* module_to_json(struct module* module, struct list* stack) {
	char crumbs[list_length(stack) * 3 + 1];

	crumbs[0] = 0;
	for (int i = 0; i < list_length(stack); i++) strcpy(crumbs + i * 3, "../");

	cJSON* root = cJSON_CreateObject();

	cJSON_AddStringToObject(root, "name", module->name);
	cJSON_AddStringToObject(root, "root", crumbs);

	if (module->desc != NULL) {
		char* desc = cmark_markdown_to_html(module->desc, strlen(module->desc), 0);
		cJSON_AddStringToObject(root, "desc", desc);
		free(desc);
	} else {
		cJSON_AddStringToObject(root, "desc", "");
	}

	cJSON* parents = cJSON_AddArrayToObject(root, "parents");
	LIST_ITER_T(stack, ancestor, const char*)
		cJSON_AddItemToArray(parents, cJSON_CreateString(ancestor));

	if (module->filename) {
		cJSON* source = cJSON_AddObjectToObject(root, "source");

		cJSON_AddStringToObject(source, "file", module->filename);
		cJSON_AddNumberToObject(source, "line", (double) module->line);
	} else {
		cJSON_AddNullToObject(root, "source");
	}

	cJSON* children = cJSON_AddArrayToObject(root, "modules");
	cJSON* items[3] = {
		cJSON_AddArrayToObject(root, "vars"),
		cJSON_AddArrayToObject(root, "funcs"),
		cJSON_AddArrayToObject(root, "classes"),
	};

	LIST_ITER_T(module->children, child, struct module*) {
		cJSON* nd = cJSON_CreateObject();

		cJSON_AddStringToObject(nd, "name", child->name);
		cJSON_AddStringToObject(nd, "desc", child->desc);

		cJSON_AddItemToArray(children, nd);
	}

	LIST_ITER_T(module->items, item, struct item*) {
		cJSON* nd = cJSON_CreateObject();

		cJSON_AddStringToObject(nd, "name", item->name);
		cJSON_AddStringToObject(nd, "desc", item->desc);

		cJSON_AddItemToArray(items[item->type], nd);
	}

	static const char* keys[4] = { "modules", "vars", "funcs", "classes" };
	for (int i = 0; i < 4; i++)
		if (cJSON_GetArraySize(cJSON_GetObjectItem(root, keys[i])) == 0)
			cJSON_ReplaceItemInObject(root, keys[i], cJSON_CreateNull());

	return root;
}

static int document_module(struct ctx ctx, struct module* module) {
	int fd = openat(
		dirfd(ctx.output), "index.html", O_CREAT | O_WRONLY | O_TRUNC, FILE_FLAGS
	);
	if (fd == -1) {
		perrorf("failed to open output");
		return 2;
	}

	FILE *file = fdopen(fd, "w");
	if (!file) {
		perrorf("failed to open output");
		return 2;
	}

	cJSON* json = module_to_json(module, ctx.stack);

	lattice_error *err = NULL;
	const char *search[] = { ctx.templates.dir, NULL };
	lattice_opts opts = { .search = search, .ignore_emit_zero = true };
	lattice_cjson_file(ctx.templates.module, json, file, opts, &err);

	fclose(file);
	cJSON_Delete(json);

	if (err) {
		perrorf("failed to render module template (%s)", err->message);

		lattice_error_code code = err->code;
		lattice_error_free(err);
		return code == LATTICE_IO_ERROR ? 2 : 3;
	}

	DIR* dir = ctx.output;
	list_push(ctx.stack, module->name);

	LIST_ITER_T(module->items, item, struct item*) {
		int ret = document_item(ctx, item);
		if (ret > 0) return ret;
	}

	LIST_ITER_T(module->children, child, struct module*) {
		if (mkdirat(dirfd(dir), child->name, DIR_FLAGS) == -1) {
			if (errno != EEXIST) {
				perrorf("failed to create output dir");
				return 2;
			}
		}

		int fd = openat(dirfd(dir), child->name, O_DIRECTORY);
		ctx.output = fdopendir(fd);

		int ret = document_module(ctx, child);
		closedir(ctx.output);
		if (ret > 0) return ret;
	}

	ctx.output = dir;
	list_pop(ctx.stack);

	return 0;
}

static cJSON* item_to_json(struct item* item, struct list* stack) {
	char crumbs[list_length(stack) * 3 + 1];

	crumbs[0] = 0;
	for (int i = 0; i < list_length(stack); i++) strcpy(crumbs + i * 3, "../");

	cJSON* root = cJSON_CreateObject();

	cJSON_AddStringToObject(root, "name", item->name);
	cJSON_AddStringToObject(root, "root", crumbs);

	static const char* types[] = { "var", "func", "class" };
	cJSON_AddStringToObject(root, "type", types[item->type]);

	if (item->desc != NULL) {
		char* desc = cmark_markdown_to_html(item->desc, strlen(item->desc), 0);
		cJSON_AddStringToObject(root, "desc", desc);
		free(desc);
	} else {
		cJSON_AddStringToObject(root, "desc", "");
	}

	cJSON* parents = cJSON_AddArrayToObject(root, "parents");
	LIST_ITER_T(stack, ancestor, const char*)
		cJSON_AddItemToArray(parents, cJSON_CreateString(ancestor));

	if (item->filename) {
		cJSON* source = cJSON_AddObjectToObject(root, "source");

		cJSON_AddStringToObject(source, "file", item->filename);
		cJSON_AddNumberToObject(source, "line", (double) item->line);
	} else {
		cJSON_AddNullToObject(root, "source");
	}

	if (item->type == ITEM_FUNC) {
		cJSON *params = cJSON_AddArrayToObject(root, "params");

		LIST_ITER_T(item->items, param, struct param*) {
			cJSON *nd = cJSON_CreateObject();

			cJSON_AddStringToObject(nd, "name", param->name);
			cJSON_AddBoolToObject(nd, "optional", param->optional);
			cJSON_AddBoolToObject(nd, "variable", param->variable);

			cJSON_AddItemToArray(params, nd);
		}
	}

	if (item->type == ITEM_CLASS) {
		cJSON* items[3] = {
			cJSON_AddArrayToObject(root, "vars"),
			cJSON_AddArrayToObject(root, "funcs"),
			cJSON_AddArrayToObject(root, "classes"),
		};

		LIST_ITER_T(item->items, child, struct item*) {
			cJSON* nd = cJSON_CreateObject();

			cJSON_AddStringToObject(nd, "name", child->name);
			cJSON_AddStringToObject(nd, "desc", child->desc);

			cJSON_AddItemToArray(items[child->type], nd);
		}
	}

	return root;
}

static int document_item(struct ctx ctx, struct item* item) {
	DIR* dir = ctx.output;

	if (item->type == ITEM_CLASS) {
		if (mkdirat(dirfd(dir), item->name, DIR_FLAGS) == -1) {
			if (errno != EEXIST) {
				perrorf("failed to create output dir");
				return 2;
			}
		}

		int fd = openat(dirfd(dir), item->name, O_DIRECTORY);
		ctx.output = fdopendir(fd);
	}

	char filename[11 + strlen(item->name)];
	if (item->type == ITEM_CLASS) {
		strcpy(filename, "index.html");
	} else {
		strcpy(filename, item->type == ITEM_VAR ? "var." : "func.");
		strcat(filename, item->name);
		strcat(filename, ".html");
	}

	int fd = openat(
		dirfd(ctx.output), filename, O_CREAT | O_WRONLY | O_TRUNC, FILE_FLAGS
	);
	if (fd == -1) {
		perrorf("failed to open output");
		return 2;
	}

	FILE *file = fdopen(fd, "w");
	if (!file) {
		perrorf("failed to open output");
		return 2;
	}

	cJSON* json = item_to_json(item, ctx.stack);

	lattice_error *err = NULL;
	const char *search[] = { ctx.templates.dir, NULL };
	lattice_opts opts = { .search = search, .ignore_emit_zero = true };
	lattice_cjson_file(ctx.templates.item, json, file, opts, &err);

	fclose(file);
	cJSON_Delete(json);

	if (err) {
		perrorf("failed to render item template (%s)", err->message);

		lattice_error_code code = err->code;
		lattice_error_free(err);
		return code == LATTICE_IO_ERROR ? 2 : 3;
	}

	if (item->type == ITEM_CLASS) {
		list_push(ctx.stack, item->name);

		LIST_ITER_T(item->items, child, struct item *) {
			int ret = document_item(ctx, child);
			if (ret > 0) return ret;
		}

		ctx.output = dir;
		list_pop(ctx.stack);
	}

	return 0;
}

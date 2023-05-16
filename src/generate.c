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
	const struct generate_options *opts;
};

static int document_module(struct ctx ctx, struct module* module);
static int document_item(struct ctx ctx, struct item* item);
static int document_list(struct ctx ctx, struct module* root);
static int document_sources(struct ctx ctx, struct source sources[]);
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
		.opts = &opts,
	};

	if (
		!ctx.templates.item ||
		!ctx.templates.list ||
		!ctx.templates.module ||
		!ctx.templates.source
	) return 2;

	int ret;

	ret = document_list(ctx, root);
	if (ret > 0) return ret;

	ret = document_sources(ctx, sources);
	if (ret > 0) return ret;

	ret = document_module(ctx, root);
	if (ret > 0) return ret;

	char *path = asprintf("%s/static/", template);
	DIR *template_static = opendir(path);
	free(path);

	if (!template_static) {
		if (errno == ENOENT) goto no_statics;

		perrorf("failed to open template static files directory");
		return 2;
	}

	struct dirent *dirent;
	char buf[8192];
	ssize_t read_result, write_result;

	while ((dirent = readdir(template_static))) {
		if (dirent->d_type != DT_REG) continue;

		int in_fd = openat(dirfd(template_static), dirent->d_name, O_RDONLY);
		int out_fd = openat(
			dirfd(ctx.output), dirent->d_name, O_CREAT | O_WRONLY | O_TRUNC, FILE_FLAGS
		);

		while ((read_result = read(in_fd, &buf[0], sizeof(buf)))) {
			if (read_result < 0) {
				perrorf("failed to copy static file");
				return 2;
			}

			write_result = write(out_fd, &buf[0], read_result);
			if (write_result < 0 || write_result != read_result) {
				perrorf("failed to copy static file");
				return 2;
			}
		}

		close(in_fd);
		close(out_fd);
	}

	closedir(template_static);

no_statics:
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

static cJSON* module_to_json(
	struct module* module,
	struct list* stack,
	const struct generate_options *opts
) {
	char crumbs[list_length(stack) * 3 + 1];

	crumbs[0] = 0;
	for (int i = 0; i < list_length(stack); i++) strcpy(crumbs + i * 3, "../");

	cJSON* root = cJSON_CreateObject();

	cJSON_AddStringToObject(root, "name", module->name);
	cJSON_AddStringToObject(root, "root", crumbs);
	cJSON_AddStringToObject(root, "library", opts->library);

	if (module->desc != NULL) {
		char* desc;
		if (opts->no_markdown) {
			size_t allocate = 12;
			for (char *ch = module->desc; *ch; ch++)
				allocate += (*ch == '<' || *ch == '>' || *ch == '&') ? 5 : 1;

			desc = malloc(allocate);
			strcpy(desc, "<pre>");

			for (size_t i = 0, j = 5; module->desc[i]; i++, j++) {
				char ch = module->desc[i];
				desc[j] = ch;

				if (ch == '<' || ch == '>' || ch == '&') {
					sprintf(desc + j, "&#%02d;", module->desc[i]);
					j += 4;
				}
			}

			strcpy(desc + allocate - 7, "</pre>");
		} else {
			desc = cmark_markdown_to_html(module->desc, strlen(module->desc), 0);
		}

		cJSON_AddStringToObject(root, "desc", desc);
		cJSON_AddStringToObject(root, "rawDesc", module->desc);

		free(desc);
	} else {
		cJSON_AddStringToObject(root, "desc", "");
		cJSON_AddStringToObject(root, "rawDesc", "");
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
		cJSON_AddStringToObject(nd, "desc", child->desc ? child->desc : "");

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

	cJSON* json = module_to_json(module, ctx.stack, ctx.opts);

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

static cJSON* item_to_json(
	struct item* item,
	struct list* stack,
	const struct generate_options *opts
) {
	int crumb_limit = list_length(stack) - (item->type != ITEM_CLASS);
	char crumbs[crumb_limit * 3 + 1];

	crumbs[0] = 0;
	for (int i = 0; i < crumb_limit; i++) strcpy(crumbs + i * 3, "../");

	cJSON* root = cJSON_CreateObject();

	cJSON_AddStringToObject(root, "name", item->name);
	cJSON_AddStringToObject(root, "root", crumbs);
	cJSON_AddStringToObject(root, "library", opts->library);

	static const char* types[] = { "var", "func", "class" };
	cJSON_AddStringToObject(root, "type", types[item->type]);

	if (item->desc != NULL) {
		char* desc;
		if (opts->no_markdown) {
			size_t allocate = 12;
			for (char *ch = item->desc; *ch; ch++)
				allocate += (*ch == '<' || *ch == '>' || *ch == '&') ? 5 : 1;

			desc = malloc(allocate);
			strcpy(desc, "<pre>");

			for (size_t i = 0, j = 5; item->desc[i]; i++, j++) {
				char ch = item->desc[i];
				desc[j] = ch;

				if (ch == '<' || ch == '>' || ch == '&') {
					sprintf(desc + j, "&#%02d;", item->desc[i]);
					j += 4;
				}
			}

			strcpy(desc + allocate - 7, "</pre>");
		} else {
			desc = cmark_markdown_to_html(item->desc, strlen(item->desc), 0);
		}

		cJSON_AddStringToObject(root, "desc", desc);
		cJSON_AddStringToObject(root, "rawDesc", item->desc);

		free(desc);
	} else {
		cJSON_AddStringToObject(root, "desc", "");
		cJSON_AddStringToObject(root, "rawDesc", "");
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

	cJSON* json = item_to_json(item, ctx.stack, ctx.opts);

	lattice_error *err = NULL;
	const char *search[] = { ctx.templates.dir, NULL };
	lattice_opts opts = { .search = search, .ignore_emit_zero = true };
	lattice_cjson_file(ctx.templates.item, json, file, opts, &err);

	fclose(file);
	cJSON_Delete(json);

	if (err) {
		fprintf(stderr, "%s %d\n", item->name, err->line);
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

static const char* type_keys[] = {
	[ITEM_VAR] = "vars",
	[ITEM_FUNC] = "funcs",
	[ITEM_CLASS] = "classes",
};

static void all_class_to_json(
	cJSON *root,
	struct item *class,
	struct list *stack
) {
	list_push(stack, class->name);

	LIST_ITER_T(class->items, item, struct item *) {
		cJSON *array = cJSON_GetObjectItem(root, type_keys[item->type]);
		cJSON *entry = cJSON_CreateArray();

		LIST_ITER_T(stack, parent, const char *) {
			cJSON_AddItemToArray(entry, cJSON_CreateString(parent));
		}
		cJSON_AddItemToArray(entry, cJSON_CreateString(item->name));

		cJSON_AddItemToArray(array, entry);

		if (item->type == ITEM_CLASS) all_class_to_json(root, item, stack);
	}

	list_pop(stack);
}

static void all_to_json(
	cJSON *root,
	struct module *module,
	struct list *stack
) {
	list_push(stack, module->name);

	LIST_ITER_T(module->items, item, struct item *) {
		cJSON *array = cJSON_GetObjectItem(root, type_keys[item->type]);
		cJSON *entry = cJSON_CreateArray();

		LIST_ITER_T(stack, parent, const char *) {
			cJSON_AddItemToArray(entry, cJSON_CreateString(parent));
		}
		cJSON_AddItemToArray(entry, cJSON_CreateString(item->name));

		cJSON_AddItemToArray(array, entry);

		if (item->type == ITEM_CLASS) all_class_to_json(root, item, stack);
	}

	LIST_ITER_T(module->children, child, struct module *) {
		all_to_json(root, child, stack);
	}

	list_pop(stack);
}

static int document_list(struct ctx ctx, struct module* root) {
	int fd = openat(
		dirfd(ctx.output), "list.html", O_CREAT | O_WRONLY | O_TRUNC, FILE_FLAGS
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

	cJSON *json = cJSON_CreateObject();
	cJSON_AddStringToObject(json, "root", "./");
	cJSON_AddStringToObject(json, "library", ctx.opts->library);
	cJSON_AddArrayToObject(json, "vars");
	cJSON_AddArrayToObject(json, "funcs");
	cJSON_AddArrayToObject(json, "classes");

	struct list *stack = list_new();
	all_to_json(json, root, stack);
	list_free(stack, NULL);

	lattice_error *err = NULL;
	const char *search[] = { ctx.templates.dir, NULL };
	lattice_opts opts = { .search = search, .ignore_emit_zero = true };
	lattice_cjson_file(ctx.templates.list, json, file, opts, &err);

	fclose(file);
	cJSON_Delete(json);

	if (err) {
		perrorf("failed to render module template (%s)", err->message);

		lattice_error_code code = err->code;
		lattice_error_free(err);
		return code == LATTICE_IO_ERROR ? 2 : 3;
	}

	return 0;
}

struct directory {
	const char *name; size_t length;
	struct list *children, *files;
};

static int dsalcmp(const void *a, const void *b) {
	return strcmp(*((const char **) a), *((const char **) b));
}

static void dir_to_json(
	cJSON *array,
	struct directory *dir,
	char *parents,
	size_t length
) {
	size_t new_length = length;
	if (dir->length > 0) {
		strncpy(parents + length, dir->name, dir->length);
		strcpy(parents + length + dir->length, "/");
		new_length += dir->length + 1;
	}

	cJSON_AddItemToArray(array, cJSON_CreateString(parents));

	LIST_ITER_T(dir->children, child, struct directory *)
		dir_to_json(array, child, parents, new_length);

	LIST_ITER_T(dir->files, file, const char *)
		cJSON_AddItemToArray(array, cJSON_CreateString(file));

	if (length > 0) free(dir);
}

static int create_source_dirs(int parent, struct directory *dir) {
	LIST_ITER_T(dir->children, child, struct directory *) {
		char *name = strndup(child->name, child->length);
		if (mkdirat(parent, name, DIR_FLAGS) == -1) {
			if (errno != EEXIST) {
				perrorf("failed to create output dir");
				return 2;
			}
		}

		int new = openat(parent, name, O_DIRECTORY);

		if (create_source_dirs(new, child) > 0) return 2;

		free(name);
		close(new);
	}

	return 0;
}

static int document_sources(struct ctx ctx, struct source sources[]) {
	cJSON *json = cJSON_CreateObject();
	cJSON_AddStringToObject(json, "root", "./");
	cJSON_AddStringToObject(json, "library", ctx.opts->library);
	cJSON_AddNullToObject(json, "path");
	cJSON_AddNullToObject(json, "contents");
	cJSON *array = cJSON_AddArrayToObject(json, "tree");

	size_t count = 0;
	for (struct source *source = sources; source->file; source++) count++;

	const char *names[count + 1];
	names[count] = NULL;

	size_t max = 0;
	for (struct source *source = sources; source->file; source++) {
		names[source - sources] = source->alias;

		size_t length = strlen(source->alias);
		if (length > max) max = length;
	}

	qsort(names, count, sizeof(const char *), dsalcmp);

	struct directory root = {
		.name = "", .length = 0,
		.children = list_new(),
		.files = list_new(),
	}, *current;

	struct list *stack = list_new();
	list_push(stack, &root);

	for (size_t j = 0; j < count; j++) {
		size_t k = 0, offset = 0;
		current = &root;

		LIST_ITER_T(stack, parent, struct directory *) {
			if (k++ == 0) continue;

			if (
				strncmp(names[j] + offset, parent->name, parent->length) ||
				strcspn(names[j] + offset, "/") != parent->length
			) {
				k--;
				break;
			}

			current = parent;
			offset += parent->length + 1;
		}

		size_t iss = list_length(stack);
		for (; k < iss; k++) list_pop(stack);

		size_t length;
		while ((length = strcspn(names[j] + offset, "/"))) {
			if (*(names[j] + offset + length)) {
				struct directory *new = malloc(sizeof(struct directory));
				new->name = names[j] + offset;
				new->length = length;
				new->children = list_new();
				new->files = list_new();

				list_push(current->children, new);
				list_push(stack, new);
				current = new;
				offset += length + 1;
			} else break;
		}

		list_push(current->files, (void *) names[j]);
	}

	list_free(stack, NULL);

	if (mkdirat(dirfd(ctx.output), "src", DIR_FLAGS) == -1) {
		if (errno != EEXIST) {
			perrorf("failed to create output dir");
			return 2;
		}
	}

	int src_fd = openat(dirfd(ctx.output), "src", O_DIRECTORY);
	if (create_source_dirs(src_fd, &root) > 0) return 2;
	close(src_fd);

	char buffer[max + 2];
	buffer[0] = 0;
	dir_to_json(array, &root, buffer, 0);

	int fd = openat(
		dirfd(ctx.output), "src.html", O_CREAT | O_WRONLY | O_TRUNC, FILE_FLAGS
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

	lattice_error *err = NULL;
	const char *search[] = { ctx.templates.dir, NULL };
	lattice_opts opts = { .search = search, .ignore_emit_zero = true };

	lattice_cjson_file(ctx.templates.source, json, file, opts, &err);
	fclose(file);

	if (err) {
		perrorf("failed to render sources template (%s)", err->message);

		lattice_error_code code = err->code;
		lattice_error_free(err);
		return code == LATTICE_IO_ERROR ? 2 : 3;
	}

	for (struct source *source = sources; source->file; source++) {
		size_t path_len = strlen(source->alias);
		char path[path_len + 10];

		strcpy(path, "src/");
		strcpy(path + 4, source->alias);
		strcpy(path + path_len + 4, ".html");

		fd = openat(dirfd(ctx.output), path, O_CREAT | O_WRONLY | O_TRUNC, FILE_FLAGS);
		if (fd == -1) {
			perrorf("failed to open output");
			return 2;
		}

		file = fdopen(fd, "w");
		if (!file) {
			perrorf("failed to open output");
			return 2;
		}

		cJSON_DeleteItemFromObject(json, "root");
		cJSON_DeleteItemFromObject(json, "path");
		cJSON_DeleteItemFromObject(json, "contents");

		char *contents = read_file(source->file);
		if (!contents) {
			perrorf("failed to read source file");
			return 2;
		}

		size_t dotdots = 0;
		for (size_t i = 0; path[i]; i++) if (path[i] == '/') dotdots++;
		char root[dotdots * 3 + 1];
		for (size_t i = 0; i < dotdots; i++) strcpy(root + i * 3, "../");

		cJSON_AddStringToObject(json, "root", root);
		cJSON_AddStringToObject(json, "path", source->alias);
		cJSON_AddStringToObject(json, "contents", contents);
		free(contents);

		lattice_cjson_file(ctx.templates.source, json, file, opts, &err);
		fclose(file);

		if (err) {
			perrorf("failed to render sources template (%s)", err->message);

			lattice_error_code code = err->code;
			lattice_error_free(err);
			return code == LATTICE_IO_ERROR ? 2 : 3;
		}
	}

	cJSON_Delete(json);
	return 0;
}

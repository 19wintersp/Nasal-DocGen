#include <ctype.h>
#include <dirent.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "generate.h"
#include "parse.h"
#include "util.h"

#ifndef NAME
#define NAME    "nasal-docgen"
#endif
#define VERSION "0.1.0"

extern char* optarg;
extern int optind, optopt;

int argc;
char* const* argv;

struct input {
	char* file;
	char* module;
};

struct options {
	struct generate_options generate;
};

int parse_options(struct options* options);
int parse_inputs(struct input inputs[], int* n_inputs);
int resolve_inputs(struct input inputs[], int n_inputs);
int process_inputs(struct input inputs[], int n_inputs, struct options opts);

int main(int _argc, char* const _argv[]) {
	argc = _argc;
	argv = _argv;

	if (argc < 1) {
		argv = (char* const[]) { NAME, NULL };
		argc = 1;
	}

	struct options options = { 0 };
	if (parse_options(&options)) return 1;

	if (!options.generate.output) options.generate.output = "docs";

	struct input inputs[1024] = {};
	int n_inputs = argc - optind;
	if (parse_inputs(inputs, &n_inputs)) return 1;
	if (resolve_inputs(inputs, n_inputs)) return 2;

	return process_inputs(inputs, n_inputs, options);
}

int parse_options(struct options* options) {
	int lastopt;
	while ((lastopt = getopt(argc, argv, ":ho:t:v")) != -1) {
		switch ((char) lastopt) {
			case 'h':
				printf("Usage: %s [OPTION]... [FILE]...\n", argv[0]);
				puts("Generate documentation for FILE(s).");
				puts("With no FILE, operate on all files in the current directory.");

				puts("");

				puts("These OPTIONs are available:");
				puts("  -h             print help information");
				puts("  -o=OUTPUT      output to directory OUTPUT");
				puts("  -t=TEMPLATE    use documentation template from TEMPLATE");
				puts("  -v             print version information");

				puts("");

				puts("Exit status:");
				puts("  0    completed successfully");
				puts("  1    argument error");
				puts("  2    system (IO) error");
				puts("  3    processing error");

				return 0;

			case 'v':
				printf("%s %s\n", NAME, VERSION);
				puts("Copyright (c) 2022 Patrick Winters");
				puts("Licensed under the GNU General Public License, version 2.");

				return 0;

			case 'o':
				if (options->generate.output != NULL) {
					fprintf(stderr, "%s: -o can only appear once\n", argv[0]);
					return 1;
				}

				options->generate.output = malloc(strlen(optarg) + 1);
				strcpy((char*) options->generate.output, optarg);

				if (options->generate.output[0] == '=') options->generate.output++;

				break;

			case 't':
				if (options->generate.template != NULL) {
					fprintf(stderr, "%s: -t can only appear once\n", argv[0]);
					return 1;
				}

				options->generate.template = malloc(strlen(optarg) + 1);
				strcpy((char*) options->generate.template, optarg);

				if (options->generate.template[0] == '=') options->generate.template++;

				break;

			case ':':
				fprintf(stderr, "%s: -%c requires a value\n", argv[0], (char) optopt);
				return 1;

			case '?':
				fprintf(stderr, "%s: -%c is not an option\n", argv[0], (char) optopt);
				return 1;
		}
	}

	return 0;
}

char buf[PATH_MAX] = { '.', '/', 0 };
int buflen = 2;

void search_dir(struct input inputs[], int* n_inputs) {
	DIR* dir = opendir(buf);
	if (dir == NULL) return;

	struct dirent* dir_item;
	while ((dir_item = readdir(dir))) {
		if (strcmp(dir_item->d_name, ".") == 0) continue;
		if (strcmp(dir_item->d_name, "..") == 0) continue;

		strcpy(buf + buflen, dir_item->d_name);

		int buflen_before = buflen;
		buflen += strlen(dir_item->d_name);

		if (dir_item->d_type == DT_DIR) {
			buf[buflen] = '/';
			buflen += 1;
			buf[buflen] = 0;

			search_dir(inputs, n_inputs);
		} else if (strcmp(buf + buflen - 4, ".nas") == 0) {
			char* buf_clone = malloc(buflen - 1);
			strcpy(buf_clone, buf + 2);

			inputs[(*n_inputs)++] = (struct input) {
				.file = buf_clone,
				.module = NULL
			};
		}

		buf[buflen_before] = 0;
		buflen = buflen_before;
	}

	closedir(dir);
}

int parse_inputs(struct input inputs[], int* n_inputs) {
	for (int i = optind; i < argc; i++) {
		const char* module = strrchr(argv[i], ':');

		if (module == NULL) {
			inputs[i - optind] = (struct input) {
				.file = argv[i],
				.module = NULL
			};
		} else {
			char* file_clone = malloc(module - argv[i] + 1);
			strncpy(file_clone, argv[i], module - argv[i]);
			file_clone[module - argv[i]] = 0;

			size_t module_len = argv[i] + strlen(argv[i]) - module - 1;
			char* module_clone = malloc(module_len + 1);
			strncpy(module_clone, module + 1, module_len);
			module_clone[module_len] = 0;

			inputs[i - optind] = (struct input) {
				.file = file_clone,
				.module = module_clone + (module_clone[0] == '.' ? 1 : 0)
			};

			bool last_was_dot = false;
			for (int j = 1; j < module_len + 1; j++) {
				if (module[j] == '.') {
					if (last_was_dot || (j == module_len && j > 1)) {
						fprintf(stderr, "%s: '%s' is invalid\n", argv[0], module + 1);
						return 1;
					}

					last_was_dot = true;
				} else {
					if (!isalnum(module[j]) && module[j] != '_') {
						fprintf(stderr, "%s: '%s' is invalid\n", argv[0], module + 1);
						return 1;
					}

					last_was_dot = false;
				}
			}
		}
	}

	if (*n_inputs == 0) {
		search_dir(inputs, n_inputs);
	}

	return 0;
}

int resolve_inputs(struct input inputs[], int n_inputs) {
	char* common = NULL;
	char* segments[64] = { 0 };
	int n_segments = 0;

	for (int i = 0; i < n_inputs; i++) {
		if (inputs[i].module == NULL) {
			char* resolved = realpath(inputs[i].file, NULL);

			if (resolved == NULL) {
				perrorf("failed to resolve '%s'", inputs[i].file);
				return 1;
			}

			resolved[0] = 0;

			char* current = resolved + 1;
			char* last_current = current;
			int current_n = 0;

			if (common == NULL) segments[n_segments++] = current;

			while ((current = strchr(current, '/')) != NULL) {
				current[0] = 0;
				current += 1;

				if (common == NULL) {
					segments[n_segments++] = current;
				} else if (
					n_segments <= current_n ||
					strcmp(segments[current_n], last_current) != 0
				) {
					n_segments = current_n;
					break;
				} else {
					current_n++;
				}

				last_current = current;
			}

			if (common == NULL) {
				n_segments -= 1;
				common = resolved;
			} else {
				if (current_n < n_segments) n_segments = current_n;

				free(resolved);
			}
		}
	}

	int prefix = 1 + n_segments;
	for (int i = 0; i < n_segments; i++)
		prefix += strlen(segments[i]);

	free(common);

	for (int i = 0; i < n_inputs; i++) {
		if (inputs[i].module == NULL) {
			char* resolved = realpath(inputs[i].file, NULL) + prefix;
			int resolved_len = strlen(resolved);

			if (strcmp(resolved + resolved_len - 4, ".nas") == 0)
				resolved[resolved_len - 4] = 0;

			for (int j = 0; resolved[j]; j++) {
				if (resolved[j] == '/')
					resolved[j] = '.';
				else if (!isalnum(resolved[j]) && resolved[j] != '_')
					resolved[j] = '_';
			}

			inputs[i].module = resolved;
		}
	}

	return 0;
}

void* filter_name_eq(void* item, void* compare) {
	return strcmp(((struct module*) item)->name, compare) == 0 ? item : NULL;
}

struct module* find_or_create_module(struct module* current, char* segment) {
	struct module* select = list_iter(current->children, filter_name_eq, segment);
	if (select == NULL) {
		select = malloc(sizeof(struct module));
		select->name = segment;
		select->desc = NULL;
		select->children = list_new();
		select->items = list_new();

		list_push(current->children, select);
	}

	return select;
}

int comp_module(const struct module** a, const struct module** b) {
	return strcmp((*a)->name, (*b)->name);
}

int comp_item(const struct item** a, const struct item** b) {
	return strcmp((*a)->name, (*b)->name);
}

void sort_items(struct list* items) {
	list_sort(items, (int (*)(const void*, const void*)) comp_item);
	LIST_ITER_T(items, item, struct item*) {
		if (item->type == ITEM_CLASS) sort_items(item->items);
	}
}

void sort_module(struct module* current) {
	list_sort(current->children, (int (*)(const void*, const void*)) comp_module);
	LIST_ITER_T(current->children, child, struct module*) {
		sort_module(child);
	}

	sort_items(current->items);
}

int process_inputs(struct input inputs[], int n_inputs, struct options opts) {
	struct module root = {
		.filename = NULL,
		.name = "",
		.desc = "", // configure this via arg
		.children = list_new(),
		.items = list_new(),
	};

	for (int i = 0; i < n_inputs; i++) {
		struct module* current = &root;
		const char* segment = inputs[i].module;
		char* next_segment;

		while ((next_segment = strchr(segment, '.')) != NULL) {
			next_segment[0] = 0;

			current = find_or_create_module(current, (char*) segment);
			segment = next_segment + 1;
		}

		current = find_or_create_module(current, (char*) segment);
		current->filename = inputs[i].file;
		current->line = 1;

		int ret = parse_file(inputs[i].file, current);
		if (ret > 0) return ret;
	}

	sort_module(&root);

	return generate_docs(&root, opts.generate);
}

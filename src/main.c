#include <ctype.h>
#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef NAME
#define NAME    "nasal-docgen"
#endif
#define VERSION "0.1.0"

extern char* optarg;
extern int optind, optopt;

int argc;
char* const* argv;

struct input {
	const char* file;
	const char* module;
};

struct options {
	const char* output;
	const char* template;
};

int parse_options(struct options* options);
int parse_inputs(struct input inputs[], int* n_inputs);
int resolve_inputs(struct input inputs[], int n_inputs);

int main(int _argc, char* const _argv[]) {
	argc = _argc;
	argv = _argv;

	if (argc < 1) {
		argv = (char* const[]) { NAME, NULL };
		argc = 1;
	}

	struct options options = { NULL, NULL };
	if (parse_options(&options)) return 1;

	options.output = options.output == NULL ? "docs" : options.output;
	options.template = options.template == NULL ? "default" : options.template;

	struct input inputs[1024] = {};
	int n_inputs = argc - optind;
	if (parse_inputs(inputs, &n_inputs)) return 1;
	if (resolve_inputs(inputs, n_inputs)) return 2;

	// todo

	return 0;
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
				if (options->output != NULL) {
					fprintf(stderr, "%s: -o can only appear once\n", argv[0]);
					return 1;
				}

				options->output = malloc(strlen(optarg) + 1);
				strcpy((char*) options->output, optarg);

				if (options->output[0] == '=') options->output += 1;

				break;

			case 't':
				if (options->template != NULL) {
					fprintf(stderr, "%s: -t can only appear once\n", argv[0]);
					return 1;
				}

				options->template = malloc(strlen(optarg) + 1);
				strcpy((char*) options->template, optarg);

				if (options->template[0] == '=') options->template += 1;

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
					if (last_was_dot || j == module_len) {
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
	for (int i = 0; i < n_inputs; i++) {
		if (inputs[i].module == NULL) {
			// todo
		}
	}

	return 0;
}

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

struct input {
	const char* file;
	const char* module;
};

int main(int argc, char* const argv[]) {
	const char* output = NULL;
	const char* template = NULL;

	if (argc < 1) {
		argv = (char* const[]) { NAME, NULL };
		argc = 1;
	}

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
				if (output != NULL) {
					fprintf(stderr, "%s: -o can only appear once\n", argv[0]);
					return 1;
				}

				output = malloc(strlen(optarg) + 1);
				strcpy((char*) output, optarg);

				break;

			case 't':
				if (template != NULL) {
					fprintf(stderr, "%s: -t can only appear once\n", argv[0]);
					return 1;
				}

				template = malloc(strlen(optarg) + 1);
				strcpy((char*) template, optarg);

				break;

			case ':':
				fprintf(stderr, "%s: -%c requires a value\n", argv[0], (char) optopt);
				return 1;

			case '?':
				fprintf(stderr, "%s: -%c is not an option\n", argv[0], (char) optopt);
				return 1;
		}
	}

	output = output == NULL ? "docs" : output;
	template = template == NULL ? "default" : template;

	if (output[0] == '=') output += 1;
	if (template[0] == '=') template += 1;

	struct input inputs[1024] = {};
	int n_inputs = argc - optind;
	bool search_dir = n_inputs == 0;

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

	if (search_dir) {
		void search_level(struct input inputs[1024], int* n_inputs);

		search_level(inputs, &n_inputs);
	}

	for (int i = 0; i < n_inputs; i++) {
		if (inputs[i].module == NULL) {
			// set default name
		}
	}

	// todo

	return 0;
}

char buf[4096] = { '.', '/', 0 };
int buflen = 2;

void search_level(struct input inputs[1024], int* n_inputs) {
	DIR* level = opendir(buf);
	if (level == NULL) return;

	struct dirent* level_item;
	while ((level_item = readdir(level))) {
		if (strcmp(level_item->d_name, ".") == 0) continue;
		if (strcmp(level_item->d_name, "..") == 0) continue;

		strcpy(buf + buflen, level_item->d_name);

		int buflen_before = buflen;
		buflen += strlen(level_item->d_name);

		if (level_item->d_type == DT_DIR) {
			buf[buflen] = '/';
			buflen += 1;
			buf[buflen] = 0;

			search_level(inputs, n_inputs);
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

	closedir(level);
}

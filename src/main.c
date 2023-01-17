#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef NAME
#define NAME    "nasal-docgen"
#endif
#define VERSION "0.1.0"

extern char* optarg;
extern int optopt;

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

	// todo

	return 0;
}

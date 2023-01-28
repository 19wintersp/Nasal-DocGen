#include <ctype.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <nasal/nasal.h>
#include <nasal/data.h>
#include <nasal/parse.h>

#include "parse.h"
#include "util.h"

extern char* const* argv;

struct line {
	const char* start;
	const char* start_nows;
	int length;
	bool doc_used;
};

static void parse_toplevel(struct Token*, struct line*, struct module*);
static void parse_object(struct Token*, struct line*, struct module*);

static struct Token* clone_token(struct Token* tok, struct Token* parent) {
	struct Token* clone = malloc(sizeof(struct Token));
	memcpy(clone, tok, sizeof(struct Token));

	if (clone->next != NULL) {
		clone->next = clone_token(clone->next, parent);
		clone->next->prev = clone;
	} else if (parent != NULL) {
		parent->lastChild = clone;
	}

	if (clone->children != NULL) {
		clone->children = clone_token(clone->children, clone);
	} else if (clone->lastChild != NULL) {
		clone->lastChild = clone_token(clone->lastChild, clone);
		clone->children = clone->lastChild;
	}

	return clone;
}

static void free_token(struct Token* tok) {
	if (tok->children != NULL) free_token(tok->children);
	else if (tok->lastChild != NULL) free_token(tok->lastChild);
	if (tok->next != NULL) free_token(tok->next);

	free(tok);
}

int parse_file(const char* rawfilename, struct module* module) {
	char* filename = realpath(rawfilename, NULL);
	if (filename == NULL) {
		perrorf("failed to resolve '%s'", rawfilename);
		return 2;
	}

	int fd = open(filename, O_RDONLY);
	if (fd == -1) {
		perrorf("failed to open '%s'", filename);
		return 2;
	}

	struct stat s;
	if (fstat(fd, &s) == -1) {
		perrorf("failed to stat '%s'", filename);
		return 2;
	}

	if ((s.st_mode & S_IFMT) != S_IFREG) {
		fprintf(stderr, "%s: '%s' is not a regular file\n", argv[0], filename);
		return 1;
	}

	char* file = malloc(s.st_size + 1);
	file[s.st_size] = 0;
	if (read(fd, file, s.st_size) == -1) {
		perrorf("failed to read '%s'", filename);
		return 2;
	}

	close(fd);

	naContext ctx = naNewContext();

	naRef filenameStr = naNewString(ctx);
	naStr_fromdata(filenameStr, filename, strlen(filename));

	int errLine;
	naRef codeRef = naParseCode(ctx, filenameStr, 1, file, s.st_size, &errLine);

	if (naIsNil(codeRef)) {
		char* err = naGetError(ctx);
		fprintf(stderr, "%s: %s:%d: %s\n", argv[0], rawfilename, errLine, err);
		return 3;
	}

	struct Token* root = (void*) PTR(codeRef).obj;

	naFreeContext(ctx);
	free(filename);

	struct line* lines = malloc(1024);
	int n_lines = 0;
	const char* current = file;

	do {
		if (n_lines > 0)
			lines[n_lines - 1].length = current - lines[n_lines - 1].start;

		lines[n_lines] = (struct line) { current, current, 0 };
		while (isblank(lines[n_lines].start_nows[0])) lines[n_lines].start_nows++;

		n_lines++;

		if (n_lines % (1024 / sizeof(struct line)) == 0)
			lines = realloc(lines, n_lines * sizeof(struct line) + 1024);
	} while ((current = strchr(current, '\n') + 1) != NULL + 1);

	lines[n_lines - 1].length = strlen(lines[n_lines - 1].start);

	parse_toplevel(root, lines, module);

	free_token(root);
	free(file);
	free(lines);

	return 0;
}

// ugly hack: overrides the call to naCodeGen at the end of naParse code
// instead of doing code generation, we'll clone and return the token tree
naRef naCodeGen(struct Parser* _p, struct Token* block, struct Token* _null) {
	naRef ret;
	SETPTR(ret, clone_token(block, NULL));

	return ret;
}

static void process_item(
	int line,
	const char* name,
	struct Token* rhs,
	struct line* lines,
	struct module* module
) {
	char* buffer = malloc(1);
	int length = 0;

	for (int i = line - 2; i >= 0; i--) {
		if (lines[i].start_nows[0] != '#') break;

		size_t hashes = strspn(lines[i].start_nows, "#");
		size_t spaces = strspn(lines[i].start_nows + hashes, "\t ");

		const char* line_end = lines[i].start + lines[i].length;
		size_t line_length = line_end - lines[i].start_nows - hashes - spaces;

		buffer = realloc(buffer, length + line_length + 1);
		memmove(buffer + line_length, buffer, length);
		strncpy(buffer, line_end - line_length, line_length);
		length += line_length;
		buffer[length - 1] = '\n';

		lines[i].doc_used = true;
	}

	buffer[length] = 0;

	//

	free(buffer);
}

static void parse_toplevel(
	struct Token* tok,
	struct line* lines,
	struct module* module
) {
	if (tok == NULL) return;

	switch (tok->type) {
		case TOK_TOP:
			parse_toplevel(tok->lastChild, lines, module);
			break;

		case TOK_SEMI:
			parse_toplevel(tok->children, lines, module);
			parse_toplevel(tok->lastChild, lines, module);
			break;

		case TOK_ASSIGN:
			if (tok->children == NULL || tok->lastChild == NULL) break;

			struct Token* chch = tok->children->children;
			if (chch == NULL) break;

			if (tok->children->type == TOK_VAR && chch->type == TOK_SYMBOL) {
				char* name = astrndup(chch->str, chch->strlen);
				process_item(chch->line, name, tok->lastChild, lines, module);
				free(name);
			} else if (
				tok->children->type == TOK_LPAR ||
				(tok->children->type == TOK_VAR && chch->type == TOK_LPAR)
			) {
				if (tok->lastChild->type != TOK_LPAR) break;
				if (tok->lastChild->children == NULL) break;

				bool expect_var = tok->children->type == TOK_LPAR;
				struct Token* lhs = expect_var ? chch : chch->children;
				struct Token* rhs = tok->lastChild->children;

				while (lhs != NULL && rhs != NULL) {
					char* name;
					struct Token* lhp = lhs->type == TOK_COMMA ? lhs->children : lhs;
					struct Token* rhp = rhs->type == TOK_COMMA ? rhs->children : rhs;

					if (lhp != NULL && rhp != NULL) {
						if (expect_var) {
							if (
								lhp->type == TOK_VAR &&
								lhp->children != NULL &&
								lhp->children->type == TOK_SYMBOL
							) name = astrndup(lhp->children->str, lhp->children->strlen);
						} else if (lhp->type == TOK_SYMBOL) {
							name = astrndup(lhp->str, lhp->strlen);
						}

						if (name != NULL) {
							process_item(lhp->line, name, rhp, lines, module);
							free(name);
						}
					}

					if (lhs->type == TOK_COMMA && lhs->lastChild) lhs = lhs->lastChild;
					else break;
					if (rhs->type == TOK_COMMA && rhs->lastChild) rhs = rhs->lastChild;
					else break;
				}
			}

			break;

		default:
			break;
	}
}

static void parse_object(
	struct Token* tok,
	struct line* lines,
	struct module* module
) {
	//
}

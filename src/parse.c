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

#include "marker.h"
#include "parse.h"
#include "util.h"

extern char* const* argv;
const char* current_file;

struct line {
	const char* start;
	const char* start_nows;
	int length;
	bool doc_used;
};

static void parse_toplevel(struct Token*, struct line*, struct module*);
static void parse_object(struct Token*, struct line*, struct list*, struct list*);
static void parse_function(struct Token*, struct list*);

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

int parse_file(const char* rawfilename, const char* fr, struct module* module) {
	current_file = fr;

	char* filename = realpath(rawfilename, NULL);
	char* file = read_file(filename);
	if (file == NULL) return 2;

	naContext ctx = naNewContext();

	naRef filenameStr = naNewString(ctx);
	naStr_fromdata(filenameStr, filename, strlen(filename));

	int errLine;
	naRef codeRef = naParseCode(ctx, filenameStr, 1, file, strlen(file), &errLine);

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
	char* name,
	struct Token* rhs,
	struct line* lines,
	struct list* module_children,
	struct list* module_items
) {
	char* desc = malloc(1);
	int length = 0;
	struct markers* markers = markers_new();

	for (int i = line - 2; i >= 0; i--) {
		if (lines[i].start_nows[0] != '#') break;

		size_t hashes = strspn(lines[i].start_nows, "#");
		size_t spaces = strspn(lines[i].start_nows + hashes, "\t ");

		const char* line_end = lines[i].start + lines[i].length;
		size_t line_length = line_end - lines[i].start_nows - hashes - spaces;

		if (*(line_end - line_length) == '@') {
			parse_marker(line_end - line_length, line_length, markers);
		} else {
			desc = realloc(desc, length + line_length + 1);
			memmove(desc + line_length, desc, length);
			strncpy(desc, line_end - line_length, line_length);
			length += line_length;
			desc[length - 1] = '\n';
		}

		lines[i].doc_used = true;
	}

	desc[length] = 0;

	if (markers->f_var + markers->f_module + markers->f_class > 1)
		markers->f_var = markers->f_module = markers->f_class = false;
	if (markers->f_public && markers->f_private)
		markers->f_public = markers->f_private = false;

	if (markers->f_private || (name[0] == '_' && !markers->f_public)) {
		free(name);
		free(desc);
	} else {
		struct item* item = malloc(sizeof(struct item));
		item->filename = current_file;
		item->line = line;
		item->name = name;
		item->desc = desc;

		list_push(module_items, item);

		if (rhs->type == TOK_LCURL && !markers->f_var) {
			if (markers->f_module && module_children != NULL) {
				list_pop(module_items);
				free(item);

				struct module* submodule = malloc(sizeof(struct module));
				submodule->filename = current_file;
				submodule->line = line;
				submodule->name = name;
				submodule->desc = desc;
				submodule->children = list_new();
				submodule->items = list_new();

				parse_object(rhs, lines, submodule->children, submodule->items);

				list_push(module_children, submodule);
			} else {
				item->type = ITEM_CLASS;
				item->items = list_new();

				parse_object(rhs, lines, NULL, item->items);
			}
		} else if (rhs->type == TOK_FUNC && !markers->f_var) {
			item->type = ITEM_FUNC;
			item->items = list_new();

			parse_function(rhs, item->items);
		} else {
			item->type = ITEM_VAR;
			item->items = NULL;
		}
	}

	markers_free(markers);
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
				process_item(
					chch->line,
					name,
					tok->lastChild,
					lines,
					module->children,
					module->items
				);
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
							process_item(
								lhp->line,
								name,
								rhp,
								lines,
								module->children,
								module->items
							);
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

static void parse_object_item(
	struct Token* tok,
	struct line* lines,
	struct list* module_children,
	struct list* module_items
) {
	if (tok == NULL || tok->type != TOK_COLON) return;
	if (!tok->children || !tok->children->next) return;
	if (tok->children->type != TOK_SYMBOL) return;

	process_item(
		tok->children->line,
		astrndup(tok->children->str, tok->children->strlen),
		tok->lastChild,
		lines, module_children, module_items
	);
}

static void parse_object(
	struct Token* tok,
	struct line* lines,
	struct list* module_children,
	struct list* module_items
) {
	struct Token* item = tok->children;

	while (item && item->type == TOK_COMMA) {
		parse_object_item(item->children, lines, module_children, module_items);
		item = item->lastChild;
	}

	parse_object_item(item, lines, module_children, module_items);
}

static void parse_param(struct Token* tok, struct list* params) {
	if (tok == NULL) return;

	struct param param = { 0 };

	switch (tok->type) {
		case TOK_SYMBOL:
			param.name = astrndup(tok->str, tok->strlen);

			break;

		case TOK_ASSIGN:
			if (tok->children && tok->children->type == TOK_SYMBOL) {
				param.name = astrndup(tok->children->str, tok->children->strlen);
				param.optional = true;

				break;
			} else return;

		case TOK_ELLIPSIS:
			if (tok->children && tok->children->type == TOK_SYMBOL) {
				param.name = astrndup(tok->children->str, tok->children->strlen);
				param.variable = true;

				break;
			} else return;

		default:
			break;
	}

	struct param* clone = malloc(sizeof(struct param));
	*clone = param;

	list_push(params, clone);
}

static void parse_function(struct Token* tok, struct list* params) {
	if (!tok->children || tok->children->type != TOK_LPAR) return;

	struct Token* item = tok->children->children;

	while (item && item->type == TOK_COMMA) {
		parse_param(item->children, params);
		item = item->lastChild;
	}

	parse_param(item, params);
}

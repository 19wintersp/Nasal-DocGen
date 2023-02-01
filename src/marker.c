#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "marker.h"
#include "parse.h"
#include "util.h"

static void parse_type(const char* type, int length, struct list** typeset);

void parse_marker(const char* line, int length, struct markers* markers) {
	size_t marker_length = strcspn(line + 1, "\t\r\n ");
	const char* marker_end = line + 1 + marker_length;

#define MARKER_MATCH(s) strncmp(line + 1, s, marker_length) == 0

	if (MARKER_MATCH("const")) {
		markers->f_readonly = true;
	} else if (MARKER_MATCH("readonly")) {
		markers->f_readonly = true;
	} else if (MARKER_MATCH("var")) {
		markers->f_var = true;
	} else if (MARKER_MATCH("module")) {
		markers->f_module = true;
	} else if (MARKER_MATCH("class")) {
		markers->f_class = true;
	} else if (MARKER_MATCH("public")) {
		markers->f_public = true;
	} else if (MARKER_MATCH("private")) {
		markers->f_private = true;
	} else if (MARKER_MATCH("constructor")) {
		markers->f_constructor = true;
	} else if (MARKER_MATCH("static")) {
		markers->f_static = true;
	} else if (MARKER_MATCH("type")) {
		const char* arg = marker_end + strspn(marker_end, "\t\r\n ");
		size_t arg_length = strcspn(arg, "\t\r\n ");

		if (arg >= line + length) return;

		parse_type(arg, arg_length, &markers->type);
	} else if (MARKER_MATCH("inherit")) {
		if (markers->inheritance == NULL) markers->inheritance = list_new();

		const char* arg = marker_end + strspn(marker_end, "\t\r\n ");
		size_t arg_length = strcspn(arg, "\t\r\n ");

		while (arg < line + length) {
			list_push(markers->inheritance, astrndup(arg, arg_length));

			arg += arg_length + strspn(arg + arg_length, "\t\r\n ");
			arg_length = strcspn(arg, "\t\r\n ");
		}
	} else if (
		MARKER_MATCH("param") || MARKER_MATCH("prop") || MARKER_MATCH("return")
	) {
		char* name;
		struct list* typeset = NULL;
		char* desc = NULL;

		const char* arg = marker_end + strspn(marker_end, "\t\r\n ");
		size_t arg_length = strcspn(arg, "\t\r\n ");

		if (arg >= line + length) return;

		if (MARKER_MATCH("param") || MARKER_MATCH("prop")) {
			name = astrndup(arg, arg_length);

			arg += arg_length + strspn(arg + arg_length, "\t\r\n ");
			arg_length = strcspn(arg, "\t\r\n ");

			if (arg >= line + length) return;
		}

		parse_type(arg, arg_length, &typeset);

		arg += arg_length + strspn(arg + arg_length, "\t\r\n ");
		arg_length = line + length - arg;

		if (arg_length > 0) desc = astrndup(arg, arg_length);

		struct marker_pair unnamed = { typeset, desc };
		struct marker_pair_named named = { name, typeset, desc };

		if (MARKER_MATCH("return")) {
			struct marker_pair* ptr = malloc(sizeof(struct marker_pair));
			*ptr = unnamed;

			if (markers->returns == NULL) markers->returns = list_new();
			list_push(markers->returns, ptr);
		} else {
			struct marker_pair_named* ptr = malloc(sizeof(struct marker_pair_named));
			*ptr = named;

			struct list** list =
				MARKER_MATCH("param") ? &markers->params : &markers->props;
			if (*list == NULL) *list = list_new();
			list_push(*list, ptr);
		}
	}

#undef MARKER_MATCH
}

/*
	grammar for type annotations:

	s = ' ' | '\t'
	type = item ( '|' item )*
	item = s* ( name | list | hash | func ) s*
	list = '[' type ']'
	hash = '{' type '}'
	func = ( '<' type '>' )? '(' args ')'
	args = type ( ',' type )*
	name = 'nil' | 'any' | 'num' | 'str' | class
*/

enum type_lexeme {
	TYPE_LEXEME_NULL,
	TYPE_LEXEME_LPAREN,
	TYPE_LEXEME_RPAREN,
	TYPE_LEXEME_LBRACK,
	TYPE_LEXEME_RBRACK,
	TYPE_LEXEME_LBRACE,
	TYPE_LEXEME_RBRACE,
	TYPE_LEXEME_LANGLE,
	TYPE_LEXEME_RANGLE,
	TYPE_LEXEME_PIPE,
	TYPE_LEXEME_COMMA,
	TYPE_LEXEME_BANG,
	TYPE_LEXEME_STAR,
	TYPE_LEXEME_IDENT,
};

struct type_token {
	enum type_lexeme lexeme;
	char* ident;
	struct type_token* next;
	struct type_token* parent;
	struct type_token* child;
};

static struct type_token* tokens_lex(char* atype) {
	struct type_token* root = malloc(sizeof(struct type_token));
	struct type_token* last = root;

	*root = (struct type_token) { TYPE_LEXEME_NULL, 0 };

	for (int i = 0; atype[i]; i++) {
		enum type_lexeme lexeme = TYPE_LEXEME_NULL;
		char* ident = NULL;

		switch (atype[i]) {
			case '(': lexeme = TYPE_LEXEME_LPAREN; break;
			case ')': lexeme = TYPE_LEXEME_RPAREN; break;
			case '[': lexeme = TYPE_LEXEME_LBRACK; break;
			case ']': lexeme = TYPE_LEXEME_RBRACK; break;
			case '{': lexeme = TYPE_LEXEME_LBRACE; break;
			case '}': lexeme = TYPE_LEXEME_RBRACE; break;
			case '<': lexeme = TYPE_LEXEME_LANGLE; break;
			case '>': lexeme = TYPE_LEXEME_RANGLE; break;
			case '|': lexeme = TYPE_LEXEME_PIPE;   break;
			case ',': lexeme = TYPE_LEXEME_COMMA;  break;
			case '!': lexeme = TYPE_LEXEME_BANG;   break;
			case '*': lexeme = TYPE_LEXEME_STAR;   break;
			default:
				if (isalpha(atype[i]) || atype[i] == '_') lexeme = TYPE_LEXEME_IDENT;
				break;
		}

		if (lexeme == TYPE_LEXEME_NULL) continue;

		if (lexeme == TYPE_LEXEME_IDENT) {
			int length = 0;

			do {
				i++;
				length++;
			} while (isalnum(atype[i]) || atype[i] == '_');

			ident = astrndup(atype + i - length, length);
			i--;
		}

		last->next = malloc(sizeof(struct type_token));
		last = last->next;
		*last = (struct type_token) { lexeme, ident, 0 };
	}

	struct type_token* old = root;
	root = root->next;
	free(old);

	return root;
}

static void tokens_regroup_brackets(struct type_token* root) {
	struct type_token* current = root;
	struct type_token* prev = current;

	while (current != NULL) {
		if (
			current->lexeme == TYPE_LEXEME_LPAREN ||
			current->lexeme == TYPE_LEXEME_LBRACK ||
			current->lexeme == TYPE_LEXEME_LBRACE ||
			current->lexeme == TYPE_LEXEME_LANGLE
		) {
			struct type_token* adopt = current->next;
			while (adopt != NULL) {
				adopt->parent = current;
				adopt = adopt->next;
			}

			current->child = current->next;
			current->next = NULL;
			prev = current;
			current = current->child;
		} else if (
			current->lexeme == TYPE_LEXEME_RPAREN ||
			current->lexeme == TYPE_LEXEME_RBRACK ||
			current->lexeme == TYPE_LEXEME_RBRACE ||
			current->lexeme == TYPE_LEXEME_RANGLE
		) {
			if (
				current->parent == NULL ||
				current->parent->lexeme + 1 != current->lexeme
			) {
				prev = current;
				current = current->next;
				continue;
			}

			struct type_token* to_free = current;

			current->parent->next = current->next;
			prev->next = NULL;
			prev = current->parent;
			current = current->next;

			struct type_token* orphan = current;
			while (orphan != NULL) {
				orphan->parent = orphan->parent->parent;
				orphan = orphan->next;
			}

			free(to_free);
		} else {
			prev = current;
			current = current->next;
		}
	}
}

static void tokens_regroup_op(struct type_token** root, enum type_lexeme op) {
	struct type_token* first = *root;

	if (first == NULL) return;

	bool has_op = false;
	for (struct type_token* cc = first; cc; cc = cc->next) {
		tokens_regroup_op(&cc->child, op);
		if (cc->lexeme == op) has_op = true;
	}

	if (!has_op) return;

	struct type_token* group = malloc(sizeof(struct type_token));
	*group = (struct type_token) { op, 0 };
	group->parent = first->parent;

	*root = group;

	struct type_token* last_child = NULL;

	for (struct type_token* cc = first; cc; cc = cc->next) {
		if (cc->lexeme == op) {
			if (last_child) last_child->next = NULL;
			last_child = NULL;

			group->next = cc;
			cc->parent = group->parent;
			group = cc;
		} else {
			if (last_child) last_child->next = cc;
			else group->child = cc;

			cc->parent = group;
			last_child = cc;
		}
	}

	if (last_child) last_child->next = NULL;
	group->next = NULL;
}

static void tokens_apply(struct type_token* tok, struct list* typeset) {
	if (tok == NULL) return;

	struct list* return_typeset = NULL;
	struct type_token* child = NULL;

	switch (tok->lexeme) {
		case TYPE_LEXEME_PIPE:
			for (child = tok; child; child = child->next)
				tokens_apply(child->child, typeset);

			break;

		case TYPE_LEXEME_LANGLE:
			if (!tok->next || tok->next->lexeme != TYPE_LEXEME_LPAREN) break;

			return_typeset = list_new();
			tokens_apply(tok->child, return_typeset);

			tok = tok->next;

		case TYPE_LEXEME_LPAREN: {}
			struct list* param_typesets = list_new();

			if (tok->child) {
				switch (tok->child->lexeme) {
					case TYPE_LEXEME_COMMA:
						for (child = tok->child; child; child = child->next) {
							struct list* param_typeset = list_new();
							tokens_apply(child->child, param_typeset);
							list_push(param_typesets, param_typeset);
						}

						break;

					default: {}
						struct list* param_typeset = list_new();
						tokens_apply(tok->child, param_typeset);
						list_push(param_typesets, param_typeset);

						break;
				}
			}

			struct type* func = malloc(sizeof(struct type));
			func->type = TYPE_FUNC;
			func->data.func = (struct bare_func) { param_typesets, return_typeset };

			list_push(typeset, func);

			break;

		case TYPE_LEXEME_LBRACK:
		case TYPE_LEXEME_LBRACE: {}
			struct list* subtype_typeset = list_new();

			if (tok->child) tokens_apply(tok->child, subtype_typeset);

			struct type* coll = malloc(sizeof(struct type));
			coll->type = tok->lexeme == TYPE_LEXEME_LBRACK ? TYPE_LIST : TYPE_HASH;
			coll->data.typeset = subtype_typeset;

			list_push(typeset, coll);

			break;

		case TYPE_LEXEME_BANG:
		case TYPE_LEXEME_STAR:
		case TYPE_LEXEME_IDENT: {}
			struct type* ident = malloc(sizeof(struct type));
			ident->type = TYPE_OBJ;

			if (tok->lexeme == TYPE_LEXEME_BANG)
				ident->type = TYPE_NIL;
			else if (tok->lexeme == TYPE_LEXEME_STAR)
				ident->type = TYPE_ANY;
			else if (strcmp(tok->ident, "nil") == 0)
				ident->type = TYPE_NIL;
			else if (strcmp(tok->ident, "any") == 0)
				ident->type = TYPE_ANY;
			else if (strcmp(tok->ident, "num") == 0)
				ident->type = TYPE_NUM;
			else if (strcmp(tok->ident, "str") == 0)
				ident->type = TYPE_STR;
			else
				ident->data.class = astrndup(tok->ident, strlen(tok->ident));

			list_push(typeset, ident);

			break;

		default:
			break;
	}
}

static void tokens_free(struct type_token* tok) {
	struct type_token* next;
	for (; tok; tok = next) {
		tokens_free(tok->child);
		next = tok->next;
		free(tok->ident);
		free(tok);
	}
}

static void parse_type(const char* type, int length, struct list** typeset) {
	if (*typeset == NULL) *typeset = list_new();
	char* atype = astrndup(type, length);

	struct type_token* root = tokens_lex(atype);

	tokens_regroup_brackets(root);
	tokens_regroup_op(&root, TYPE_LEXEME_COMMA);
	tokens_regroup_op(&root, TYPE_LEXEME_PIPE);

	tokens_apply(root, *typeset);

	tokens_free(root);
	free(atype);
}

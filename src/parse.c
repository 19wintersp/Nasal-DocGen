#include <fcntl.h>
#include <limits.h>
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

static struct Token* clone_token(struct Token* tok) {
	struct Token* clone = malloc(sizeof(struct Token));
	memcpy(clone, tok, sizeof(struct Token));

	if (clone->next != NULL)
		clone->next = clone_token(clone->next);
	if (clone->children != NULL)
		clone->children = clone_token(clone->children);
	else if (clone->lastChild != NULL)
		clone->lastChild = clone_token(clone->lastChild);

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

	//

	free(file);
	free_token(root);

	return 0;
}

// ugly hack: overrides the call to naCodeGen at the end of naParse code
// instead of doing code generation, we'll clone and return the token tree
naRef naCodeGen(struct Parser* _p, struct Token* block, struct Token* _null) {
	naRef ret;
	SETPTR(ret, clone_token(block));

	return ret;
}

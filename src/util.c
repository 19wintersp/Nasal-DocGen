#include <stdarg.h>
#include <stdio.h>

extern char* const* argv;

char* vasprintf(const char* format, va_list list1) {
	va_list list2;
	va_copy(list2, list1);

	int length = vsnprintf(NULL, 0, format, list1);
	va_end(list1);

	char* buffer = malloc(length + 1);
	vsnprintf(buffer, length + 1, format, list2);
	va_end(list2);

	return buffer;
}

char* asprintf(const char* format, ...) {
	va_list list1;
	va_start(list1, format);

	return vasprintf(format, list1);
}

void perrorf(const char* format, ...) {
	va_list list1;
	va_start(list1, format);

	char* message = vasprintf(format, list1);
	int length = strlen(argv[0]) + strlen(message) + 3;

	char* buffer = malloc(length);
	snprintf(buffer, length, "%s: %s", argv[0], message);

	perror(buffer);

	free(message);
	free(buffer);
}

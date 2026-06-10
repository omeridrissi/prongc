#include "log.h"

/* This function shouldn't be used in final commits. 
 * Ideally, it should only be when debugging locally */
void print_debug(const char *format, ...)
{
	va_list args;
	va_start(args, format);

	printf("[dbg] ");
	vprintf(format, args);

	va_end(args);
}

/* This function should be used when printing information
 * that otherwise wouldn't be printed if the '--verbose' option 
 * isn't selected in the command line arguments */
void print_verbose(const char *format, ...)
{
	va_list args;
	va_start(args, format);

	printf("[ver] ");
	vprintf(format, args);

	va_end(args);
}

/* This should be used for printing fatal errors that 
 * result in prongc exitting */
void print_warn(const char *format, ...)
{
	va_list args;
	va_start(args, format);

	fprintf(stderr, "\033[30;33m[warn]\033[0m ");
	vprintf(format, args);

	va_end(args);
}

/* This should be used for printing fatal errors that 
 * result in prongc exitting */
void print_error(const char *format, ...)
{
	va_list args;
	va_start(args, format);

	fprintf(stderr, "\033[37;41m[err]\033[0m ");
	vprintf(format, args);

	va_end(args);
}

void print_source_line(CXSourceLocation loc) {
	CXFile file;
	unsigned line, column, offset;
	clang_getSpellingLocation(loc, &file, &line, &column, &offset);

	CXString filename = clang_getFileName(file);
	const char *path = clang_getCString(filename);

	FILE *fp = fopen(path, "r");
	if (!fp) {
		clang_disposeString(filename);
		return;
	}

	char buf[1024];
	for (unsigned i = 1; i <= line; i++) {
		if (!fgets(buf, sizeof(buf), fp)) break;
	}

	if (buf[0]) {
		buf[strcspn(buf, "\r\n")] = '\0';
	
		int num_tabs = 0;
		for (size_t i = 0; i < 1024; ++i) {
			if (buf[i] == '\n' || buf[i] == '\0')
				break;
			if (buf[i] == '\t') {
				num_tabs++;
			}
		}

		printf("%*s%s%s%s\n", num_tabs, "\t", CLR_VAR, buf+num_tabs, CLR_RESET);
		printf("%*s%*s%s^%s\n", num_tabs, "\t", column-num_tabs-1, "", CLR_ARROW, CLR_RESET);
	}

	fclose(fp);
	clang_disposeString(filename);
}

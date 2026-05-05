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
void print_error(const char *format, ...)
{
	va_list args;
	va_start(args, format);

	fprintf(stderr, "\033[37;41m[err]\033[0m ");
	vprintf(format, args);

	va_end(args);
}



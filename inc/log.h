#include <stdio.h>
#include <stdarg.h>
#include <clang-c/Index.h>
#include <string.h>

#define TEE  "├──"
#define PIPE "│  "
#define END  "└──"

#define BRIGHT_BOLD_RED "\033[1;91m"
#define BRIGHT_BOLD_YELLOW "\033[1;33m" 

#define CLR_RESET   "\033[0m"
#define CLR_FUNC    "\033[1;36m"   // Bold Cyan
#define CLR_ARROW   BRIGHT_BOLD_YELLOW   // Bold Yellow
#define CLR_LOC     "\033[0;34m"   // Blue
#define CLR_READ    "\033[0;32m"   // Green
#define CLR_WRITE   "\033[0;31m"   // Red
#define CLR_PTRREAD "\033[0;92m"   // Bright Green
#define CLR_PTRWRITE BRIGHT_BOLD_RED  // Bright Red
#define CLR_ESCAPE  "\033[0;35m"   // Magenta
#define CLR_VAR     "\033[1;37m"   // Bold White

#define CLR_UNPROTECTED BRIGHT_BOLD_RED
#define CLR_UNCERTAIN	BRIGHT_BOLD_YELLOW

void print_verbose(const char *format, ...);
void print_debug(const char *format, ...);
void print_warn(const char *format, ...);
void print_error(const char *format, ...);
void print_source_line(CXSourceLocation loc);


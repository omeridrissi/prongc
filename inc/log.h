#include <stdio.h>
#include <stdarg.h>

#define TEE  "├──"
#define PIPE "│  "
#define END  "└──"

#define CLR_RESET   "\033[0m"
#define CLR_FUNC    "\033[1;36m"   // Bold Cyan
#define CLR_ARROW   "\033[1;33m"   // Bold Yellow
#define CLR_LOC     "\033[0;34m"   // Blue
#define CLR_READ    "\033[0;32m"   // Green
#define CLR_WRITE   "\033[0;31m"   // Red
#define CLR_PTRREAD "\033[0;92m"   // Bright Green
#define CLR_PTRWRITE "\033[0;91m"  // Bright Red
#define CLR_ESCAPE  "\033[0;35m"   // Magenta
#define CLR_VAR     "\033[1;37m"   // Bold White

void print_verbose(const char *format, ...);
void print_debug(const char *format, ...);
void print_warn(const char *format, ...);
void print_error(const char *format, ...);

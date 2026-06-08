#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <clang-c/Index.h>
#include "types.h"

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
#define DEFAULT_NUM_CURSORS 5

extern bool arg_verbose;
extern bool arg_help;

void process_tu_array(CXTranslationUnit *tu_array, 
		      struct prong_priv *client_data);
void process_func_info(FuncInfo *func_info,
			struct prong_priv *client_data);
void unwind_func_info(FuncInfo *func_info,
			struct prong_priv *client_data,
			DynamicAOS *parsed_func_call);

void build_var_access_footprint(FuncInfo *func_info, VarAccessArr *access_footprint);

void trace_va_overlap(FuncInfo *func_info,
		      VarAccess *var_access,
		      DynamicAOS *call_trace);

struct prong_priv *prong_init_priv();
void prong_free_priv(struct prong_priv *prong_priv);

error_t parse_func_call(const char *input, DynamicAOS *out);

error_t process_args(int argc, char **argv, 
		     struct prong_priv *prong_priv);

void print_usage(const char *prog_name);


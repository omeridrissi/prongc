#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <clang-c/Index.h>
#include "types.h"

#define DEFAULT_NUM_CURSORS 5

extern bool arg_verbose;
extern bool arg_help;

CXTranslationUnit	*alloc_tu_array(int length);
CXCursor		*alloc_cursor_array(int length);

void process_tu_array(CXTranslationUnit *tu_array, 
		      int length, 
		      struct prong_priv *client_data);
void process_func_cursor(CXCursor func_cursor,
			 struct prong_priv *prong_priv);
void process_func_info(FuncInfo *func_info,
			struct prong_priv *client_data);

struct prong_priv *prong_init_priv();
void prong_free_priv(struct prong_priv *prong_priv);

error_t process_args(int argc, char **argv, 
		     struct prong_priv *prong_priv);

void print_usage(const char *prog_name);

void print_verbose(const char *format, ...);
void print_debug(const char *format, ...);
void print_error(const char *format, ...);

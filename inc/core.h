#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <glob.h>
#include <clang-c/Index.h>
#include "types.h"

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

void find_va_overlap(FuncInfo *func_info_i, 
		     FuncInfo *func_info_j,
		     struct prong_priv *prong_priv);

char *mark_postfix_func_name(char *postfix, char **arg_name);
void find_exclusive_va_names(FuncInfo *func_info, struct prong_priv *client_data);

bool is_usr_param_postfix(const char *usr, const char *param_name);

struct prong_priv *prong_init_priv();
void prong_free_priv(struct prong_priv *prong_priv);

prong_error_t parse_func_call(const char *input, DynamicAOS *out);

prong_error_t process_args(int argc, char **argv, 
		     struct prong_priv *prong_priv);

void print_usage(const char *prog_name);


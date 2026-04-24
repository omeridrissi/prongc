#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <clang-c/Index.h>
#include "types.h"

#define FUNC_INFO_INIT_CAP 8

extern bool arg_verbose;
extern bool arg_help;

FuncInfo *init_func_info(CXCursor *cursor,
			 const char *usr, 
			 const char *elem_name);
void free_func_info(FuncInfo *func_info);

void push_func_info(FuncInfoArrPtr func_info_array,
		    size_t *fi_array_count,
		    size_t *fi_array_capacity,
		    FuncInfo *func_info);

FuncInfoArr init_func_info_array(size_t initial_cap);
void free_func_info_array(FuncInfoArr func_info_array);

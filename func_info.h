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

void push_func_info(FuncInfoArr *func_info_array,
		    CXCursor *cursor,
		    const char *usr, 
		    const char *elem_name);

FuncInfo *func_info_array_tail(FuncInfoArr *func_info_array);
FuncInfo *func_info_array_head(FuncInfoArr *func_info_array);

void print_func_info(FuncInfo *func_info, int indentation);
void print_func_info_array(FuncInfoArr *func_info_array, int depth);

FuncInfoArr *init_func_info_array();
void free_func_info_array(FuncInfoArr *func_info_array);

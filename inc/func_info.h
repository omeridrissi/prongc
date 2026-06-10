#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <clang-c/Index.h>
#include "types.h"

#define FUNC_INFO_INIT_CAP 8

extern bool arg_verbose;
extern bool arg_help;

extern FuncInfo empty_func_info_global;

FuncInfo *init_func_info(CXCursor *cursor,
			 const char *usr, 
			 const char *elem_name,
			 bool in_system_header,
			 bool has_definition);
void free_func_info(FuncInfo *func_info);

FuncInfo *get_null_func_info();
bool func_info_is_null(FuncInfo *func_info);

void push_func_info(FuncInfoArr *func_info_array,
		    CXCursor *cursor,
		    const char *usr, 
		    const char *elem_name,
		    bool in_system_header,
		    bool has_definition);

void print_func_info(FuncInfo *func_info, int indentation);

FuncInfoArr *init_func_info_array();
void free_func_info_array(FuncInfoArr *func_info_array);

FuncInfo *get_func_info_by_usr(FuncInfoArr *func_info_array, char *usr);

void print_func_info_array(FuncInfoArr *func_info_array, int indentation);

FuncInfo *func_info_array_tail(FuncInfoArr *func_info_array);
FuncInfo *func_info_array_head(FuncInfoArr *func_info_array);


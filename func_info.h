#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <clang-c/Index.h>
#include "types.h"

#define FUNC_INFO_INIT_CAP 8

FuncInfo *init_func_info(void);
void free_func_info(FuncInfo *func_info);
FuncInfo **init_func_info_array(size_t initial_cap);

/* This header file should contain the definitions, library includes
 * and declarations necessary for handling Dynamic Arrays of Strings */

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"

#define AOS_INITIAL_CAP 64

DynamicAOS	*init_aos(void);
void		free_aos(DynamicAOS *array);
void		reset_aos(DynamicAOS **array);

error_t		aos_push_string(DynamicAOS *array, const char *str);
ssize_t		aos_string_count(DynamicAOS *array);
bool		aos_contains_string(DynamicAOS *array, const char *needle);
ssize_t		aos_find_string_idx(DynamicAOS *array, const char *needle);
char		*aos_find_string(DynamicAOS *array, const char *needle);
char		*aos_string_at(DynamicAOS *array, size_t idx);

void		aos_print_strings(DynamicAOS *array);

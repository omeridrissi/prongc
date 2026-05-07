#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"

#define CURSOR_ARR_INIT_CAP 8

CXCursorArr	*init_cursor_array();
void		free_cursor_array(CXCursorArr *var_access_array);

void		push_cursor(CXCursorArr *cursor_array, CXCursor *cursor);

bool		in_cursor_branch(CXCursor branch, CXCursor cursor);

void		print_cursor_array(CXCursorArr *cursor_array);

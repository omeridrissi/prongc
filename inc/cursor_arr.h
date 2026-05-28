#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <clang-c/Index.h>

#include "types.h"

#define CURSOR_ARR_INIT_CAP 8
#define ANCESTOR_NOT_FOUND SIZE_MAX

typedef struct {
	CXCursor cursor;
	enum CXCursorKind kind;
	size_t offset;
	bool found;
} ClosestAncestorResult;

CXCursorArr	*init_cursor_array();
void		free_cursor_array(CXCursorArr *var_access_array);

void		push_cursor(CXCursorArr *cursor_array, CXCursor *cursor);
void		pop_cursor(CXCursorArr *cursor_array);

bool		in_cursor_branch(CXCursor branch, CXCursor cursor);
bool		cursors_are_equal(CXCursor a, CXCursor b);
bool		in_cursor_stack(CXCursorArr *stack, CXCursor cursor);
CXCursor	get_cursor_first_child(CXCursor cursor);
ClosestAncestorResult	find_closest_ancestor(CXCursorArr *stack,
					      enum CXCursorKind *target_kind,
					      size_t num_kinds);

CXCursor	get_cursor_of_kind(CXCursorArr *stack, enum CXCursorKind target_kind);
CXCursor	get_binop_assignment(CXCursorArr *stack);
size_t		get_cursor_offset_of_kind(CXCursorArr *array, enum CXCursorKind target_kind);
size_t		get_binop_assignment_offset(CXCursorArr *stack);
CXCursor	get_cursor_by_offset(CXCursorArr *stack, size_t offset);

CXCursor	get_cursor_array_tail(CXCursorArr *cursor_array);

void		print_cursor(CXCursor cursor);
void		print_cursor_array(CXCursorArr *cursor_array);

CXTranslationUnit	*alloc_tu_array(int length);
void			free_tu_array(CXTranslationUnit *tu_array, int length);



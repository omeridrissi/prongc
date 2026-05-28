#include "cursor_arr.h"
#include "log.h"

CXCursorArr *init_cursor_array()
{
	CXCursorArr *cursor_array;
	cursor_array = malloc(sizeof(*cursor_array));

	cursor_array->size = 0;
	cursor_array->capacity = CURSOR_ARR_INIT_CAP;
	cursor_array->data = malloc(sizeof(*cursor_array)*CURSOR_ARR_INIT_CAP);
	memset(cursor_array->data, '\0', sizeof(*cursor_array)*CURSOR_ARR_INIT_CAP);

	return cursor_array;
}

void free_cursor_array(CXCursorArr *cursor_array)
{
	free(cursor_array->data);
	free(cursor_array);
}

void push_cursor(CXCursorArr *cursor_array, CXCursor *cursor)
{
	if ((cursor_array->size+1)*sizeof(CXCursor) > cursor_array->capacity) {
		cursor_array->capacity *= 2;
		cursor_array->data = reallocarray(cursor_array->data,
						cursor_array->capacity,
						sizeof(CXCursor));
	}
	cursor_array->data[cursor_array->size++] = *cursor;
}

void pop_cursor(CXCursorArr *cursor_array)
{
	if (cursor_array->size == 0)
		return;
	cursor_array->size--;
}

typedef struct {
	CXCursor cursor;
	bool boolean;
} CursorBoolPair;

static enum CXChildVisitResult in_cursor_branch_visitor(CXCursor cursor,
							CXCursor parent,
							CXClientData data)
{
	(void)parent; // Casted to void cuz unused
	CursorBoolPair *pair = (CursorBoolPair*)data;
	if (clang_equalCursors(cursor, pair->cursor)) {
		pair->boolean = true;
		return CXChildVisit_Break;
	}
	return CXChildVisit_Recurse;
}

bool in_cursor_branch(CXCursor branch, CXCursor cursor)
{
	if (clang_equalCursors(branch, cursor)) return true;
	CursorBoolPair pair = {.cursor = cursor, .boolean = false};
	clang_visitChildren(branch, in_cursor_branch_visitor, &pair);
	return pair.boolean;
}

bool cursors_are_equal(CXCursor a, CXCursor b) 
{
	if (clang_equalCursors(a, b))
		return true;

	if (clang_Cursor_isNull(a) || clang_Cursor_isNull(b))
		return false;

	CXSourceLocation locA = clang_getCursorLocation(a);
	CXSourceLocation locB = clang_getCursorLocation(b);

	if (!clang_equalLocations(locA, locB))
		return false;

	CXSourceRange rangeA = clang_getCursorExtent(a);
	CXSourceRange rangeB = clang_getCursorExtent(b);

	CXSourceLocation endA = clang_getRangeEnd(rangeA);
	CXSourceLocation endB = clang_getRangeEnd(rangeB);

	return clang_equalLocations(endA, endB);
}

bool in_cursor_stack(CXCursorArr *stack, CXCursor cursor)
{
	for (size_t i = stack->size-1; i < stack->size; --i) {
		if (cursors_are_equal(stack->data[i], cursor))
			return true;
	}
	return false;
}

static enum CXChildVisitResult get_first_child_visitor(CXCursor cursor,
						     CXCursor parent,
						     CXClientData data)
{
	(void)parent;
	CXCursor *result = (CXCursor *)data;
	*result = cursor;
	return CXChildVisit_Break;
}

CXCursor get_cursor_first_child(CXCursor cursor)
{
	CXCursor result = clang_getNullCursor();
	clang_visitChildren(cursor, get_first_child_visitor, &result);
	return result;
}

static bool operator_is_assignment(CXCursor cursor) {
	enum CXBinaryOperatorKind opcode = clang_getCursorBinaryOperatorKind(cursor);
	if (clang_getCursorKind(cursor) != CXCursor_BinaryOperator) {
		print_warn("Tried to check if non-BinaryOperator cursor was an assignment\n");
		return false;
	}
	/* If it's any type of assignment */
	switch (opcode) {
		case CXBinaryOperator_Assign:       // =
        	case CXBinaryOperator_AddAssign:    // +=
        	case CXBinaryOperator_SubAssign:    // -=
        	case CXBinaryOperator_MulAssign:    // *=
        	case CXBinaryOperator_DivAssign:    // /=
        	case CXBinaryOperator_RemAssign:    // %=
        	case CXBinaryOperator_ShlAssign:    // <<=
        	case CXBinaryOperator_ShrAssign:    // >>=
        	case CXBinaryOperator_AndAssign:    // &=
        	case CXBinaryOperator_XorAssign:    // ^=
        	case CXBinaryOperator_OrAssign:     // |=
			return true;
			break;
        	default:
			return false;  // Not an assignment operator
	}
}

CXCursor get_cursor_of_kind(CXCursorArr *stack, 
			    enum CXCursorKind target_kind)
{
	for (size_t i = stack->size-1; i < stack->size; --i) {
		if (clang_getCursorKind(stack->data[i]) == target_kind)
			return stack->data[i];
	}
	return clang_getNullCursor();
}

CXCursor get_binop_assignment(CXCursorArr *stack)
{
	for (size_t i = stack->size-1; i < stack->size; --i) {
		if (clang_getCursorKind(stack->data[i]) == CXCursor_BinaryOperator &&
		    operator_is_assignment(stack->data[i]))
			return stack->data[i];
	}
	return clang_getNullCursor();
}

ClosestAncestorResult find_closest_ancestor(CXCursorArr *stack,
					    enum CXCursorKind *kinds,
					    size_t num_kinds)
{
	ClosestAncestorResult res = {.found = false, .offset = ANCESTOR_NOT_FOUND};

	for (size_t i = stack->size-1; i < stack->size; --i) {
		for (size_t j = 0; j < num_kinds; j++) {
			if (clang_getCursorKind(stack->data[i]) == kinds[j]) {
				res.found = true;
				res.cursor = stack->data[i];
				res.offset = stack->size - i;
				res.kind = kinds[j];
				return res;
			}
		}
	}

	return res;
}

/* Traverses the array in reverse, and returns
 * base offest since. A cursor's base offset inside
 * the ancestry stack is it's index from the 
 * end of the array in reverse.
 * This function returns the closest base offset of
 * cursor of target kind on success, returns -1 on 
 * failure. */
size_t get_cursor_offset_of_kind(CXCursorArr *stack, 
				 enum CXCursorKind target_kind)
{
	for (size_t i = stack->size-1; i < stack->size; --i) {
		if (clang_getCursorKind(stack->data[i]) == target_kind)
			return stack->size-i;
	}
	
	return ANCESTOR_NOT_FOUND;
}

size_t get_binop_assignment_offset(CXCursorArr *stack)
{
	for (size_t i = stack->size-1; i < stack->size; --i) {
		if (clang_getCursorKind(stack->data[i]) == CXCursor_BinaryOperator &&
		    operator_is_assignment(stack->data[i]))
			return stack->size-i;
	}
	
	return ANCESTOR_NOT_FOUND;
	
}

/* Just get the cursor with base offset */
CXCursor get_cursor_by_offset(CXCursorArr *stack, size_t offset)
{
	if (offset > stack->size-1)
		return clang_getNullCursor();
	return stack->data[stack->size-offset-1];
}

/* Get the top of the cursor stack or 'tail' of cursor array */
CXCursor get_cursor_array_tail(CXCursorArr *cursor_array) {
	return cursor_array->data[cursor_array->size-1];
}

void print_cursor(CXCursor cursor) 
{
	if (clang_Cursor_isNull(cursor)) {
		printf("NULL cursor\n");
		return;
	}

	CXString cursor_name = clang_getCursorDisplayName(cursor);
	CXString cursor_usr = clang_getCursorUSR(cursor);
	printf("\n");
	printf("Printing cusor of name: %s\n", clang_getCString(cursor_name));
	printf("cursor usr: %s\n", clang_getCString(cursor_usr));
	printf("cursor kind: %d\n", clang_getCursorKind(cursor));
	printf("cursor xdata: %d\n", cursor.xdata);
	for (int i = 0; i < 3; ++i) {
		printf("cursor.data[%d] = %p\n", i, cursor.data[i]);
	}
	printf("\n");

	clang_disposeString(cursor_name);
	clang_disposeString(cursor_usr);
}

void print_cursor_array(CXCursorArr *cursor_array)
{
	printf("======Printing cursor array of size %zu======\n", cursor_array->size);
	for (size_t i = 0; i < cursor_array->size; ++i) {
		print_cursor(cursor_array->data[i]);
	}
}

/* Allocates an array of CXTranslationUnit structs */
CXTranslationUnit *alloc_tu_array(int length) 
{
	CXTranslationUnit *tus;
	tus = (CXTranslationUnit*)malloc(sizeof(CXTranslationUnit) * length);

	return tus;
}

void free_tu_array(CXTranslationUnit *tu_array, int length)
{
	for (int i = 0; i < length; ++i) {
		clang_disposeTranslationUnit(tu_array[i]);
	}
}


#include "cursor_arr.h"
#include "log.h"

CXCursorArr *init_cursor_array()
{
	CXCursorArr *cursor_array;
	cursor_array = malloc(sizeof(*cursor_array));

	cursor_array->size = 0;
	cursor_array->capacity = CURSOR_ARR_INIT_CAP;
	cursor_array->data = malloc(sizeof(*cursor_array)*CURSOR_ARR_INIT_CAP);

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

	memcpy(cursor_array->data+cursor_array->size,
			cursor, sizeof(CXCursor));
	cursor_array->size++;
}

void print_cursor_array(CXCursorArr *cursor_array)
{
	for (int i = 0; i < cursor_array->size; ++i) {
		CXString cursor_name = clang_getCursorDisplayName(cursor_array->data[i]);
		printf("cursor name: %s\n", clang_getCString(cursor_name));
		printf("cursor kind: %d\n", clang_getCursorKind(cursor_array->data[i]));
	}
}

#include "func_info.h"
#include "dyn_aos.h"
#include "core.h"

FuncInfo *init_func_info(CXCursor *cursor,
			 const char *usr, 
			 const char *elem_name)
{
	FuncInfo *func_info;
	func_info = malloc(sizeof(*func_info));

	memset(func_info, '\0', sizeof(*func_info));

	if (usr != NULL)
		func_info->usr = strdup(usr);
	if (elem_name != NULL)
		func_info->name = strdup(elem_name);

	if (cursor != NULL) {
		func_info->cursor = cursor;
	}

	func_info->params = init_aos();
	func_info->locals = init_aos();

	return func_info;
}

void free_func_info(FuncInfo *func_info)
{
	free(func_info->usr);
	free(func_info->name);
	free_aos(func_info->params);
	free_aos(func_info->locals);

	free(func_info);
}

void push_func_info(FuncInfoArr *func_info_array,
		    CXCursor *cursor, const char *usr,
		    const char *elem_name)
{
	FuncInfo *func_info = init_func_info(cursor,
					     usr,
					     elem_name);

	if (func_info_array->size+sizeof(FuncInfo) > func_info_array->capacity) {
		func_info_array->capacity *= 1.5f;
		func_info_array->data = reallocarray(func_info_array->data,
						func_info_array->capacity,
						sizeof(FuncInfo*));
	}

	printf("Pushing function info struct:\n");
	printf(" USR: %s\n", func_info->usr);
	printf(" display name: %s\n", func_info->name);
	printf(" parameters: ");
	aos_print_strings(func_info->params);
	printf("\n");
	printf(" local vars: ");
	aos_print_strings(func_info->locals);
	printf("\n");

	func_info_array->size += sizeof(FuncInfo);
	memcpy(func_info_array->data+func_info_array->size,
			func_info, sizeof(*func_info));

	free(func_info);
}

FuncInfo *func_info_array_head(FuncInfoArr *func_info_array)
{
	return &func_info_array->data[0];
}

FuncInfo *func_info_array_tail(FuncInfoArr *func_info_array)
{
	size_t tail_idx = func_info_array->size-1;
	return &func_info_array->data[tail_idx];
}

FuncInfoArr *init_func_info_array()
{
	
	FuncInfoArr *func_info_array;
	func_info_array = malloc(sizeof(*func_info_array));

	func_info_array->capacity = FUNC_INFO_INIT_CAP;
	func_info_array->size = 0;
	func_info_array->data = malloc(sizeof(FuncInfo)*FUNC_INFO_INIT_CAP);

	return func_info_array;
}

void free_func_info_array(FuncInfoArr *func_info_array)
{
	free(func_info_array->data);
	free(func_info_array);
}

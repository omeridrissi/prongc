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
		memcpy(&func_info->cursor, cursor, sizeof(CXCursor));
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

void push_func_info(FuncInfoArrPtr func_info_array,
		    size_t *fi_array_count,
		    size_t *fi_array_capacity,
		    FuncInfo *func_info)
{
	if (*fi_array_count+1 > *fi_array_capacity) {
		*fi_array_capacity *= 2;
		*func_info_array = reallocarray(*func_info_array,
						*fi_array_capacity,
						sizeof(FuncInfo*));
	}

	if (arg_verbose) {
		print_verbose("Pushing function info struct:\n");
		print_verbose(" USR: %s\n", func_info->usr);
		print_verbose(" display name: %s\n", func_info->name);
		print_verbose(" parameters: ");
		aos_print_strings(func_info->params);
		printf("\n");
		print_verbose(" local vars: ");
		aos_print_strings(func_info->locals);
		printf("\n");
	}

	*func_info_array[++*fi_array_count] = func_info;
}

FuncInfo **init_func_info_array(size_t initial_cap)
{
	FuncInfo **array;
	array = malloc(sizeof(*array)*initial_cap);

	memset(array, '\0', sizeof(*array)*initial_cap);

	return array;
}

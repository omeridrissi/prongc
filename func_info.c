#include "func_info.h"

FuncInfo *init_func_info(char *usr, char *elem_name)
{
	FuncInfo *func_info;
	func_info = malloc(sizeof(*func_info));

	memset(func_info, '\0', sizeof(*func_info));

	if (usr != NULL)
		func_info->usr = strdup(usr);
	if (elem_name != NULL)
		func_info->name = strdup(elem_name);

	func_info->params = aos_init(void);
	func_info->locals = aos_init(void);

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

FuncInfo **init_func_info_array(size_t initial_cap)
{
	FuncInfo **array;
	array = malloc(sizeof(*array)*initial_cap);

	memset(array, '\0', sizeof(*array)*initial_cap);

	return array;
}

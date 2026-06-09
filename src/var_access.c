#include <stdint.h>
#include "var_access.h"
#include "log.h"

#define NO_IDX SIZE_MAX

VarAccess *init_var_access(const char *usr, const char *name,
			   const char *esc_func_usr, size_t esc_param_idx,
			   int line, int column, VarAccessType type,
			   bool is_ptr_type)
{
	VarAccess *var_access;
	var_access = malloc(sizeof(*var_access));

	if (usr != NULL)
		var_access->usr = strdup(usr);

	if (name != NULL)
		var_access->name = strdup(name);

	var_access->line = line;
	var_access->column = column;
	var_access->type = type;

	var_access->esc_param_idx = esc_param_idx;

	if (esc_func_usr != NULL)
		var_access->esc_func_usr = strdup(esc_func_usr);
	else
		var_access->esc_func_usr = NULL;

	var_access->is_ptr_type = is_ptr_type;

	return var_access;
}

void free_var_access(VarAccess *var_access) 
{
	free(var_access->usr);
	free(var_access->name);
	if (var_access->esc_func_usr)
		free(var_access->esc_func_usr);

	free(var_access);
}

VarAccessArr *init_var_access_array()
{
	VarAccessArr *var_access_array;
	var_access_array = malloc(sizeof(*var_access_array));

	var_access_array->size = 0;
	var_access_array->capacity = VAR_ACC_ARR_INIT_CAP;
	var_access_array->data = malloc(sizeof(*var_access_array)*VAR_ACC_ARR_INIT_CAP);

	return var_access_array;
}

void free_var_access_array(VarAccessArr *var_access_array)
{
	free(var_access_array->data);
	free(var_access_array);
}

void push_var_access(VarAccessArr *var_access_array,
			const char *usr, const char *name, 
			const char *esc_func_usr, size_t esc_param_idx, 
			int line, int column, VarAccessType type,
			bool is_ptr_type)
{
	VarAccess *var_access = init_var_access(usr, name, 
						esc_func_usr, 
						esc_param_idx, line,
						column, type, is_ptr_type);

	if ((var_access_array->size+1)*sizeof(VarAccess) > var_access_array->capacity) {
		var_access_array->capacity *= 2;
		var_access_array->data = reallocarray(var_access_array->data,
						var_access_array->capacity,
						sizeof(VarAccess));
	}

	memcpy(var_access_array->data+var_access_array->size,
			var_access, sizeof(VarAccess));
	var_access_array->size++;

	free(var_access);

}

void push_access_copy(VarAccessArr *var_access_array, VarAccess *var_access)
{
	if ((var_access_array->size+1)*sizeof(VarAccess) > var_access_array->capacity) {
		var_access_array->capacity *= 2;
		var_access_array->data = reallocarray(var_access_array->data,
						var_access_array->capacity,
						sizeof(VarAccess));
	}

	memcpy(var_access_array->data+var_access_array->size,
			var_access, sizeof(VarAccess));
	var_access_array->size++;
}

void print_var_access(VarAccess *var_access, int indentation)
{
	int x = indentation*3;
	printf("%*s|_Variable access: \n", x, "");
	printf("%*s|   name: %s\n", x, "", var_access->name);
	printf("%*s|   usr: %s\n", x, "", var_access->usr);
	printf("%*s|   line: %d\n", x, "", var_access->line);
	printf("%*s|   col: %d\n", x, "", var_access->column);
	printf("%*s|   type: ", x, "");
	switch (var_access->type) {
		case VarAccess_Read:
			printf("read\n");
			break;
		case VarAccess_Write:
			printf("write\n");
			break;
		case VarAccess_PtrRead:
			printf("read from memory location\n");
			break;
		case VarAccess_PtrWrite:
			printf("write to memory location\n");
			break;
		case VarAccess_Escape:
			printf("passed to function as parameter\n");
			break;
		case VarAccess_Null:
			printf("NULL\n");
			break;
	}
	if (var_access->esc_func_usr) 
		printf("%*s|   esc func usr: %s\n", x, "", var_access->esc_func_usr);
	
	if (var_access->esc_param_idx != NO_IDX)
		printf("%*s|   esc param idx: %zu\n", x, "", var_access->esc_param_idx);

	if (var_access->is_ptr_type) 
		printf("%*s|   *is pointer\n", x, "");
	else
		printf("%*s|   *not pointer\n", x, "");
	
}

bool equal_var_accesses(VarAccess *va_a, VarAccess *va_b) 
{
	return (strcmp(va_a->usr, va_b->usr) == 0);
}

void print_var_access_array(VarAccessArr *var_access_array, int indentation) 
{
	for (size_t i = 0; i < var_access_array->size; ++i) {
		print_var_access(&var_access_array->data[i], indentation);
	}
}

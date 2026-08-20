#include <stdint.h>
#include "var_access.h"
#include "log.h"

#define NO_IDX SIZE_MAX

VarAccess *init_var_access(const char *usr, const char *name,
			   const char *esc_func_spelling,
			   const char *esc_func_usr, char *parent_func_name,
			   unsigned int esc_param_idx,
			   unsigned int line, unsigned int column, VarAccessType type,
			   bool is_ptr_type)
{
	VarAccess *var_access;
	var_access = malloc(sizeof(*var_access));

	if (usr != NULL)
		var_access->usr = strdup(usr);
	else 
		var_access->usr = NULL;

	if (name != NULL)
		var_access->name = strdup(name);
	else
		var_access->name = NULL;

	var_access->parent_func_name = parent_func_name;

	var_access->line = line;
	var_access->column = column;
	var_access->type = type;

	var_access->esc_param_idx = esc_param_idx;

	if (esc_func_usr != NULL)
		var_access->esc_func_usr = strdup(esc_func_usr);
	else
		var_access->esc_func_usr = NULL;

	if (esc_func_spelling != NULL)
		var_access->esc_func_spelling = strdup(esc_func_spelling);
	else
		var_access->esc_func_spelling = NULL;

	if (type == VarAccess_Call) {
		var_access->usr = var_access->esc_func_usr;
		var_access->name = var_access->esc_func_spelling;
	}

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
			const char *esc_func_spelling,
			const char *esc_func_usr, char *parent_func_name,
			unsigned int esc_param_idx, 
			unsigned int line, unsigned int column, VarAccessType type,
			bool is_ptr_type)
{
	VarAccess *var_access = init_var_access(usr, name, esc_func_spelling,
						esc_func_usr, parent_func_name,
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

static bool va_array_contains(VarAccessArr *var_access_array, VarAccess *var_access) 
{
	for (size_t i = 0; i < var_access_array->size; ++i) {
		VarAccess *working_va = &var_access_array->data[i];
		if (equal_var_access_structs(working_va, var_access)) 
			return true;
	}
	return false;
}

void push_access_copy(VarAccessArr *var_access_array, VarAccess *var_access)
{
	//if (va_array_contains(var_access_array, var_access))
	//	return;

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
		case VarAccess_LockAcquire:
			printf("acquire lock\n");
			break;
		case VarAccess_LockRelease:
			printf("release lock\n");
			break;
		case VarAccess_Call:
			printf("function call\n");
			break;
		case VarAccess_IfStmt:
			printf("if statement\n");
			break;
		case VarAccess_ElseStmt:
			printf("else statement\n");
			break;
		case VarAccess_EndIf:
			printf("endif\n");
			break;
		case VarAccess_Null:
			printf("NULL\n");
			break;
		default:
			printf("\n");
			break;
	}
	if (var_access->esc_func_spelling) 
		printf("%*s|   esc func usr: %s\n", x, "", var_access->esc_func_spelling);
	
	if (var_access->esc_param_idx != NO_IDX)
		printf("%*s|   esc param idx: %u\n", x, "", var_access->esc_param_idx);

	if (var_access->is_ptr_type) 
		printf("%*s|   *is pointer\n", x, "");
	else
		printf("%*s|   *not pointer\n", x, "");
	
}

bool equal_var_accesses(VarAccess *va_a, VarAccess *va_b) 
{
	return (strcmp(va_a->usr, va_b->usr) == 0);
}

bool equal_var_access_structs(VarAccess *va_a, VarAccess *va_b) 
{
	return (memcmp(va_a, va_b, sizeof(VarAccess)) == 0);
}

bool va_is_placeholder_type(VarAccessType va_type)
{
	return  (va_type == VarAccess_Null) || 
		(va_type >= VarAccess_Call && va_type <= VarAccess_EndIf);
}

void print_var_access_array(VarAccessArr *var_access_array, int indentation) 
{
	for (size_t i = 0; i < var_access_array->size; ++i) {
		print_var_access(&var_access_array->data[i], indentation);
	}
}

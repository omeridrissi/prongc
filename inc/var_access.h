#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"

#define VAR_ACC_ARR_INIT_CAP 8

VarAccess	*init_var_access(const char *usr, const char *name,
				 const char *esc_func_usr, size_t esc_func_idx,
				 int line, int column, VarAccessType type, bool is_ptr_type);
void		free_var_access(VarAccess *var_access);

VarAccessArr	*init_var_access_array();
void		free_var_access_array(VarAccessArr *var_access_array);

void		push_var_access(VarAccessArr *var_access_array,
				const char *usr, const char *name, 
				const char *esc_func_usr, size_t esc_func_idx,
				int line, int column, VarAccessType type, bool is_ptr_type);

void		push_access_copy(VarAccessArr *var_access_array, VarAccess *var_access);

bool		equal_var_accesses(VarAccess *va_a, VarAccess *va_b);
bool		equal_var_access_structs(VarAccess *va_a, VarAccess *va_b);

void		print_var_access(VarAccess *var_access, 
				 int indentation);
void		print_var_access_array(VarAccessArr *var_access_array, 
				       int indentation);


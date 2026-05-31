#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"

#define VAR_ACC_ARR_INIT_CAP 8

VarAccess	*init_var_access(const char *usr, const char *name,
				 const char *esc_func_usr,
				 int line, int column, VarAccessType type);
void		free_var_access(VarAccess *var_access);

VarAccessArr	*init_var_access_array();
void		free_var_access_array(VarAccessArr *var_access_array);

void		push_var_access(VarAccessArr *var_access_array,
				const char *usr, const char *name, 
				const char *esc_func_usr, int line,
				int column, VarAccessType type);

void print_var_access(VarAccess *var_access, 
		      int indentation);
void print_var_access_array(VarAccessArr *var_access_array, 
			    int indentation);


#include "core.h"
#include "dyn_aos.h"
#include "func_info.h"
#include "var_access.h"
#include "cursor_arr.h"
#include "log.h"

/* Visitor function for matching our specified 
 * function in args to our desired function 
 * definition in the AST */
static enum CXChildVisitResult prong_visitor_walk_ast(CXCursor current_cursor, 
					    CXCursor parent_cursor, 
					    CXClientData client_data)
{

	/* Skip cursors that exist inside system header files */
	CXSourceLocation cursor_location = clang_getCursorLocation(current_cursor);
	if (clang_Location_isInSystemHeader(cursor_location)) {
		return CXChildVisit_Continue;
	}

	struct prong_priv *prong_priv = (struct prong_priv *)client_data;

	CXString parent_display_name = clang_getCursorDisplayName(parent_cursor);
	
	enum CXCursorKind current_cursor_kind = clang_getCursorKind(current_cursor);
	enum CXCursorKind parent_cursor_kind = clang_getCursorKind(parent_cursor);

	const char *elem_name = clang_getCString(parent_display_name);

	/* Collects function declaration cursors */
	if ((current_cursor_kind == CXCursor_CompoundStmt) &&
	    (parent_cursor_kind == CXCursor_FunctionDecl)) {
		
		if (aos_contains_string(prong_priv->func_names, elem_name)) {
			CXString cursor_usr = clang_getCursorUSR(parent_cursor);
			if (arg_verbose) {
				print_verbose("Visiting element %s\n", 
					      clang_getCString(parent_display_name));
				print_verbose("	kind: %d\n", 
					      clang_getCursorKind(parent_cursor));
				print_verbose("	unified symbol representation: %s\n", 
					      clang_getCString(cursor_usr));

			}
			
			push_func_info(prong_priv->funcs,
					&parent_cursor,
					clang_getCString(cursor_usr),
					clang_getCString(parent_display_name),
					false);

			clang_disposeString(cursor_usr);
		}
		clang_disposeString(parent_display_name);
		return CXChildVisit_Continue;
	}

	/* Collects global variable USRs */
	if (current_cursor_kind == CXCursor_VarDecl) { 
		enum CXLinkageKind current_cursor_linkage = 
			clang_getCursorLinkage(current_cursor);
		/* Fish out static and extern variables */
		if (current_cursor_linkage == CXLinkage_External ||
		    current_cursor_linkage == CXLinkage_Internal) {
			CXString cursor_usr = clang_getCursorUSR(current_cursor);

			const char *usr_string = clang_getCString(cursor_usr);
			aos_push_string(prong_priv->global_usrs, usr_string);

			clang_disposeString(cursor_usr);
		}
		clang_disposeString(parent_display_name);

		return CXChildVisit_Continue;
	}

	clang_disposeString(parent_display_name);

	return CXChildVisit_Recurse;
}

/* Visitor function for looping through 
 * the children of the compound statement cursor 
 * relating to the function we've filtered 
 * out from the AST */
static enum CXChildVisitResult prong_visitor_walk_func(CXCursor current_cursor,
						       CXCursor parent_cursor,
						       CXClientData client_data)
{
	struct prong_priv *prong_priv = (struct prong_priv*)client_data;
	
	enum CXCursorKind current_cursor_kind = clang_getCursorKind(current_cursor);
	enum CXLinkageKind current_cursor_linkage = clang_getCursorLinkage(current_cursor);

	/* What we have to unfortunately do because 
	 * libclang is a piece of shit (manually building ancestry stack)*/
	if (clang_Cursor_isNull(prong_priv->last_visited)) {
		push_cursor(prong_priv->ancestry_stack, &parent_cursor);
	} else {
		if (!clang_Cursor_isNull(parent_cursor) &&
		    cursors_are_equal(parent_cursor, prong_priv->last_visited)) {
			push_cursor(prong_priv->ancestry_stack, &parent_cursor);
		} else {
			while (prong_priv->ancestry_stack->size > 0) {
				CXCursor tail = get_cursor_array_tail(prong_priv->ancestry_stack);
				if (cursors_are_equal(tail, parent_cursor)) {
					break;
				}

				pop_cursor(prong_priv->ancestry_stack);
			}
		}
	}

	prong_priv->last_visited = current_cursor;

	if (current_cursor_linkage == CXLinkage_NoLinkage) {
		CXString current_cursor_usr = clang_getCursorUSR(current_cursor);
		switch (current_cursor_kind) {
			case CXCursor_VarDecl:
				aos_push_string(prong_priv->current_func->locals, 
						clang_getCString(current_cursor_usr));
				break;
			case CXCursor_ParmDecl:
				aos_push_string(prong_priv->current_func->params,
						clang_getCString(current_cursor_usr));
				break;
			default:
				
		}
		clang_disposeString(current_cursor_usr);
	}


	// If declrefexpr is the descendant of the last child of the array
	// of pushed cursors, the 
	if (current_cursor_kind == CXCursor_DeclRefExpr) {
		FuncInfo *current_func = prong_priv->current_func;
		if (!current_func->var_accesses)
			current_func->var_accesses = init_var_access_array();
		
		//CXCursor parent_cursor = clang_getCursorLexicalParent(current_cursor);
		/* Check if this is an assignment */
		//enum CXCursorKind refexpr_parent_kind = clang_getCursorKind(parent_cursor);

		CXCursor referenced_cursor = clang_getCursorReferenced(current_cursor);
		CXString decl_usr = clang_getCursorUSR(referenced_cursor);
		CXString decl_name = clang_getCursorDisplayName(referenced_cursor);
		
		if (clang_getCursorKind(referenced_cursor) != CXCursor_VarDecl &&
		    clang_getCursorKind(referenced_cursor) != CXCursor_ParmDecl)
			goto visitor_recurse;

		CXCursor closest_binop_ancestor = get_binop_assignment(prong_priv->ancestry_stack);
		CXCursor closest_unop_ancestor = get_cursor_of_kind(prong_priv->ancestry_stack, 
								    CXCursor_UnaryOperator);
		CXCursor closest_arrsubexp_ancestor = get_cursor_of_kind(prong_priv->ancestry_stack,
									 CXCursor_ArraySubscriptExpr);
		CXCursor closest_callexpr_ancestor = get_cursor_of_kind(prong_priv->ancestry_stack,
									CXCursor_CallExpr);
		CXCursor non_assign_binop_ancestor = get_cursor_of_kind(prong_priv->ancestry_stack,
									CXCursor_BinaryOperator);
		
		size_t closest_binop_offset = get_binop_assignment_offset(prong_priv->ancestry_stack);
		size_t closest_unop_offset = get_cursor_offset_of_kind(prong_priv->ancestry_stack, 
								       CXCursor_UnaryOperator);
		size_t closest_arrsubexp_offset = get_cursor_offset_of_kind(prong_priv->ancestry_stack,
									    CXCursor_ArraySubscriptExpr);
		size_t closest_callexpr_offset = get_cursor_offset_of_kind(prong_priv->ancestry_stack,
									    CXCursor_CallExpr);
		size_t non_assign_binop_offset = get_cursor_offset_of_kind(prong_priv->ancestry_stack,
									  CXCursor_BinaryOperator);

		CXSourceLocation location = clang_getCursorLocation(current_cursor);
		CXString current_filename;
		unsigned current_line, current_column;
		
		clang_getPresumedLocation(location, &current_filename, 
					  &current_line, &current_column);

		if (closest_callexpr_offset < closest_binop_offset &&
		    closest_callexpr_offset < closest_arrsubexp_offset) {
			CXType referenced_type = clang_getCursorType(current_cursor);
			CXCursor callexpr_decl = clang_getCursorReferenced(closest_callexpr_ancestor);
			CXString callexpr_name = clang_getCursorDisplayName(callexpr_decl);
			
			if (closest_unop_offset < closest_callexpr_offset &&
			    closest_unop_offset < closest_arrsubexp_offset) {
				enum CXUnaryOperatorKind unop_kind = 
					clang_getCursorUnaryOperatorKind(closest_unop_ancestor);
				
				if (unop_kind == CXUnaryOperator_AddrOf) {
					// Push as Escape
					push_var_access(current_func->var_accesses,
							clang_getCString(decl_usr),
							clang_getCString(decl_name),
							clang_getCString(callexpr_name), 
							current_line, 
							current_column, 
							VarAccess_Escape);
						
				} else {
					// Push as Read
					push_var_access(current_func->var_accesses,
							clang_getCString(decl_usr),
							clang_getCString(decl_name),
							NULL, current_line, current_column, 
							VarAccess_Read);
				}
			} else if (referenced_type.kind == CXType_Pointer) {
				// Push as Escape
				push_var_access(current_func->var_accesses,
						clang_getCString(decl_usr),
						clang_getCString(decl_name),
						clang_getCString(callexpr_name), 
						current_line, 
						current_column, 
						VarAccess_Escape);
				
			} else {
				// Push as Read
				push_var_access(current_func->var_accesses,
						clang_getCString(decl_usr),
						clang_getCString(decl_name),
						NULL, current_line, current_column, 
						VarAccess_Read);
			}
			clang_disposeString(callexpr_name);
			goto visitor_recurse;
		}

		if (closest_arrsubexp_offset < closest_binop_offset && 
		    closest_arrsubexp_offset < closest_unop_offset) {
			// Check if our cursor is on the rhs or lhs of 
			// the ArraySubscriptExpr
			CXCursor arrsubexp_lhs = get_cursor_first_child(closest_arrsubexp_ancestor);
			if (in_cursor_stack(prong_priv->ancestry_stack, arrsubexp_lhs)) {
				// Check if it's on lhs of binary operator
				if (closest_binop_offset != ANCESTOR_NOT_FOUND) {
					CXCursor binop_lhs = get_cursor_first_child(closest_binop_ancestor);
					if (in_cursor_stack(prong_priv->ancestry_stack, binop_lhs)) {
						// Push as write
						push_var_access(current_func->var_accesses,
								clang_getCString(decl_usr),
								clang_getCString(decl_name),
								NULL, current_line, current_column, 
								VarAccess_PtrWrite);
					} else {
						// Push as read
						push_var_access(current_func->var_accesses,
								clang_getCString(decl_usr),
								clang_getCString(decl_name),
								NULL, current_line, current_column, 
								VarAccess_PtrRead);
					}	
				}
			} else {
				// Push as read
				push_var_access(current_func->var_accesses,
						clang_getCString(decl_usr),
						clang_getCString(decl_name),
						NULL, current_line, current_column, 
						VarAccess_Read);
			}
		} else if (closest_unop_offset < closest_binop_offset && 
			   closest_unop_offset < closest_arrsubexp_offset) {
			// Check if it's a dereference, then if closest_binop_offset
			// exists and it's an assignment, then pass it as a PtrRead or PtrWrite.
			enum CXUnaryOperatorKind unop_kind = 
				clang_getCursorUnaryOperatorKind(closest_unop_ancestor);

			if (unop_kind == CXUnaryOperator_Deref) {
				if (closest_binop_offset != ANCESTOR_NOT_FOUND) {
					CXCursor binop_lhs = 
						get_cursor_first_child(closest_binop_ancestor);
					CXCursor non_assign_binop_lhs =
						get_cursor_first_child(non_assign_binop_ancestor);
					// Push as write if LHS
					if (in_cursor_stack(prong_priv->ancestry_stack, binop_lhs)) {
						if (in_cursor_stack(prong_priv->ancestry_stack, 
									non_assign_binop_lhs)) {
							push_var_access(current_func->var_accesses,
									clang_getCString(decl_usr),
									clang_getCString(decl_name),
									NULL, current_line, current_column, 
									VarAccess_PtrWrite);	
						} else {
							push_var_access(current_func->var_accesses,
									clang_getCString(decl_usr),
									clang_getCString(decl_name),
									NULL, current_line, current_column, 
									VarAccess_Read);
						}
					} else {
						// Push as read if not
						push_var_access(current_func->var_accesses,
								clang_getCString(decl_usr),
								clang_getCString(decl_name),
								NULL, current_line, current_column, 
								VarAccess_PtrRead);
					}
				}
			} else if (unop_kind == CXUnaryOperator_PreInc ||
				   unop_kind == CXUnaryOperator_PostInc ||
				   unop_kind == CXUnaryOperator_PreDec ||
				   unop_kind == CXUnaryOperator_PostDec) {
				// Definitely push as write
				push_var_access(current_func->var_accesses,
						clang_getCString(decl_usr),
						clang_getCString(decl_name),
						NULL, current_line, current_column, 
						VarAccess_Write);
			} else {
				// Push as read
				push_var_access(current_func->var_accesses,
						clang_getCString(decl_usr),
						clang_getCString(decl_name),
						NULL, current_line, current_column, 
						VarAccess_Read);
			}
			
		} else if (closest_binop_offset < closest_unop_offset && 
			   closest_binop_offset < closest_arrsubexp_offset) {
			// Just check if current cursor is on lhs or rhs
			CXCursor binop_lhs = get_cursor_first_child(closest_binop_ancestor);

			// Push as write if LHS
			if (in_cursor_stack(prong_priv->ancestry_stack, binop_lhs)) {
				// Push as write
				push_var_access(current_func->var_accesses,
						clang_getCString(decl_usr),
						clang_getCString(decl_name),
						NULL, current_line, current_column, 
						VarAccess_Write);
			} else {
				// Push as read if not
				push_var_access(current_func->var_accesses,
						clang_getCString(decl_usr),
						clang_getCString(decl_name),
						NULL, current_line, current_column, 
						VarAccess_Read);
			}
			
		} else	{
			push_var_access(current_func->var_accesses,
					clang_getCString(decl_usr),
					clang_getCString(decl_name),
					NULL, current_line, current_column, 
					VarAccess_Read);

		}
		
		clang_disposeString(decl_usr);
		clang_disposeString(decl_name);
		clang_disposeString(current_filename);
		goto visitor_recurse;
	}

	/* If we have a function call, push that function's
	 * definition onto the list of callees in our current
	 * FuncInfo struct, then recursively process that 
	 * function */
	if (current_cursor_kind == CXCursor_CallExpr) {
		CXCursor callee_decl = clang_getCursorReferenced(current_cursor);
		if (!clang_Cursor_isNull(callee_decl)) {
			CXCursor callee_def = clang_getCursorDefinition(callee_decl);
			CXCursor working_cursor = callee_def;

			if (clang_Cursor_isNull(callee_def))
				working_cursor = callee_decl;

			CXString callee_usr = clang_getCursorUSR(working_cursor);
			CXString callee_name = clang_getCursorDisplayName(working_cursor);

			// Make sure we don't fall into an
			// infinite recursion loop
			if (aos_contains_string(prong_priv->touched_func_usrs,
						clang_getCString(callee_usr))) {
				clang_disposeString(callee_usr);
				clang_disposeString(callee_name);
				push_cursor(prong_priv->ancestry_stack, &current_cursor);
				goto visitor_recurse;
			}

			aos_push_string(prong_priv->touched_func_usrs,
					clang_getCString(callee_usr));

			// Checks if callee array is initialized
			if (!prong_priv->current_func->callees)
				prong_priv->current_func->callees = 
					init_func_info_array();
			
			CXSourceLocation callee_location = 
				clang_getCursorLocation(working_cursor);
			
			push_func_info(
				prong_priv->current_func->callees,
				&working_cursor,
				clang_getCString(callee_usr),
				clang_getCString(callee_name),
				clang_Location_isInSystemHeader(callee_location));
			
			clang_disposeString(callee_usr);
			clang_disposeString(callee_name);
			
			if (func_info_array_tail(
			    prong_priv->current_func->callees)->in_system_header) {
				push_cursor(prong_priv->ancestry_stack, &current_cursor);
				goto visitor_recurse;
			}
			process_func_info(func_info_array_tail(
						prong_priv->current_func->callees
					  ),
					  client_data);

		}
		push_cursor(prong_priv->ancestry_stack, &current_cursor);
		goto visitor_recurse;	
	}

visitor_recurse:
	return CXChildVisit_Recurse;
}

/* Gets root cursor and visit it's children with
 * prong_visitor_walk_ast() function*/
void process_tu_array(CXTranslationUnit *tu_array, 
		      struct prong_priv *client_data) 
{
	for (size_t i = 0; i < client_data->file_names->count; ++i) {
		CXCursor cursor = clang_getTranslationUnitCursor(tu_array[i]); 

		/* Find our desired function declaration cursors and push them to
		 * our cursor list inside of client_data */
		clang_visitChildren(cursor, 
				    prong_visitor_walk_ast, 
				    (void*)client_data);
	}

}

/* Recursively visits and processes FuncInfo struct,
 * construct it's callee FuncInfo structs, visit their
 * cursor's children and construct VarAccess structs
 * for each FuncInfo. */
void process_func_info(FuncInfo *func_info,
			struct prong_priv *client_data)
{
	FuncInfo *prev_func_info = client_data->current_func;
	client_data->current_func = func_info;
	
	clang_visitChildren(func_info->cursor,
			    prong_visitor_walk_func,
			    (void*)client_data);
	client_data->current_func = prev_func_info;
}

/* Maps all escaped variables and parameters to their
 * callee's variable accesses and nulls out unnecessary
 * VarAccess structs (like those of pointer type variables 
 * that never escape)
 * - definition of escaped variables in
 * types.h VarAccessType enum definition*/
void unwind_func_info(FuncInfo *func_info,
			struct prong_priv *client_data)
{
	if (func_info->callees) {
		for (size_t i = 0; i < func_info->callees->size; ++i) {
			unwind_func_info(&func_info->callees->data[i],
					 client_data);
		}
	}
}

/* Initializes the state we're going to be
 * working with. Zeroes it out, but then allocates
 * and sets the dynamic string arrays corresponding
 * to our global and local USRs */
struct prong_priv *prong_init_priv() 
{
	struct prong_priv *prong_priv;

	prong_priv = malloc(sizeof(*prong_priv));
	if (!prong_priv) 
		goto exit;

	memset(prong_priv, '\0', sizeof(struct prong_priv));

	prong_priv->func_names = init_aos();
	prong_priv->file_names = init_aos();

	prong_priv->global_usrs = init_aos();

	prong_priv->ancestry_stack = init_cursor_array();

	prong_priv->funcs = init_func_info_array();
	prong_priv->touched_func_usrs = init_aos();


	return prong_priv;

exit:
	return NULL;
}

/* Frees prong_priv struct and it's allocated fields */
void prong_free_priv(struct prong_priv *prong_priv) 
{
	if (prong_priv->func_names)
		free_aos(prong_priv->func_names);
	if (prong_priv->file_names)
		free_aos(prong_priv->file_names);

	if (prong_priv->global_usrs)
		free_aos(prong_priv->global_usrs);

	if (prong_priv->funcs)
		free_func_info_array(prong_priv->funcs);

	if (prong_priv->touched_func_usrs)
		free_aos(prong_priv->touched_func_usrs);

	if (prong_priv->ancestry_stack)
		free_cursor_array(prong_priv->ancestry_stack);
	
	if (prong_priv)
		free(prong_priv);
	
}

/* Turns comma-separated strings into a 
 * dynamic string array DynamicAOS */
static void split_comma_list(char *str_in, DynamicAOS *dyn_aos)
{
	char *files_str_array = strdup(str_in);
	char *temp = files_str_array;
	size_t num_files = 1;
	int offset = 0;
	while (temp[offset] != '\0') {
		if (temp[offset] == ';') {
			temp[offset] = '\0';
			num_files++;
		}
		++offset;
	}

	for (size_t n = 0; n < num_files; ++n) {
		aos_push_string(dyn_aos, temp);
		temp += strlen(temp)+1;
	}

	free(files_str_array);

}

#define FILES_ARG_STRLEN 8
#define FUNCS_ARG_STRLEN 12

/* Take the necessary information provided in command line
 * and save it in state (prong_priv/client_data struct) 
 * I hate processing strings */
error_t process_args(int argc, char **argv, 
		  struct prong_priv *prong_priv)
{
	char *cmd_arg;

	for (int i = 0; i < argc; ++i) {
		cmd_arg	= argv[i];
		
		if (strncmp(cmd_arg, "--files=", FILES_ARG_STRLEN) == 0) {
			split_comma_list(cmd_arg+FILES_ARG_STRLEN, prong_priv->file_names);
		} else if (strncmp(cmd_arg, "--functions=", FUNCS_ARG_STRLEN) == 0) {
			split_comma_list(cmd_arg+FUNCS_ARG_STRLEN, prong_priv->func_names);
		} else if (strcmp(cmd_arg, "--verbose") == 0) {
			arg_verbose = true;
		} else if (strcmp(cmd_arg, "--help") == 0) {
			arg_help = true;
		} else {
			continue;
		}
	}

	if (prong_priv->func_names->count == 0 ||
		prong_priv->file_names->count == 0)
		return ERR_INVALID_ARG;

	return 0;
}

void print_usage(const char *prog_name) {
    printf("Usage: %s --files=\"file1;file2;...\" --functions=\"func1;func2;...\" [OPTIONS]\n", prog_name);
    printf("\n");
    printf("Required arguments:\n");
    printf("  --files=LIST        Comma-separated list of input source files\n");
    printf("  --functions=LIST    Comma-separated list of function names\n");
    printf("\n");
    printf("Optional arguments:\n");
    printf("  --verbose           Enable verbose output\n");
    printf("  --help              Show this help message and exit\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s --files=main.c,util.c --functions=init,run\n", prog_name);
    printf("  %s --files=a.c,b.c,c.c --functions=foo,bar --verbose\n", prog_name);
}


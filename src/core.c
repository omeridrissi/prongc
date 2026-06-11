#include <ctype.h>
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
	CXString parent_spelling = clang_getCursorSpelling(parent_cursor);

	enum CXCursorKind current_cursor_kind = clang_getCursorKind(current_cursor);
	enum CXCursorKind parent_cursor_kind = clang_getCursorKind(parent_cursor);

	const char *elem_spelling = clang_getCString(parent_spelling);

	/* Collects function declaration cursors */
	if ((current_cursor_kind == CXCursor_CompoundStmt) &&
	    (parent_cursor_kind == CXCursor_FunctionDecl)) {
		for (size_t i = 0; i < prong_priv->func_names->count; ++i) {
			char *func_name = prong_priv->func_names->strings[i];
			DynamicAOS *parsed_func_name = init_aos();

			parse_func_call(func_name, parsed_func_name);

			if (strcmp(parsed_func_name->data, elem_spelling) == 0) {
				size_t num_args = (size_t)clang_Cursor_getNumArguments(parent_cursor);

				if (num_args > parsed_func_name->count-1) {
					print_error("too few arguments to function %s, expected %zu, have %zu\n", 
							parsed_func_name->strings[0], 
							num_args, 
							parsed_func_name->count-1);
					free_aos(parsed_func_name);
					continue;
				} 

				if (num_args < parsed_func_name->count-1) {
					print_error("too many arguments to function %s, expected %zu, have %zu\n", 
							parsed_func_name->strings[0], 
							num_args, 
							parsed_func_name->count-1);
					free_aos(parsed_func_name);
					continue;
				}

				CXString cursor_usr = clang_getCursorUSR(parent_cursor);
				if (arg_verbose) {
					print_verbose("Found function CXCursor: %s\n", 
						      clang_getCString(parent_display_name));
					print_verbose("	Kind: %d\n", 
						      clang_getCursorKind(parent_cursor));
					print_verbose("	USR: %s\n", 
						      clang_getCString(cursor_usr));
	
				}
				
				push_func_info(prong_priv->funcs,
						&parent_cursor,
						clang_getCString(cursor_usr),
						clang_getCString(parent_display_name),
						false, true);
	
				clang_disposeString(cursor_usr);
			}
		
			free_aos(parsed_func_name);
		}

		clang_disposeString(parent_display_name);
		clang_disposeString(parent_spelling);
		
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
		clang_disposeString(parent_spelling);

		return CXChildVisit_Continue;
	}

	clang_disposeString(parent_display_name);
	clang_disposeString(parent_spelling);

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
			default: break;
				
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
//		size_t non_assign_binop_offset = get_cursor_offset_of_kind(prong_priv->ancestry_stack,
//									  CXCursor_BinaryOperator);

		CXSourceLocation location = clang_getCursorLocation(current_cursor);
		CXString current_filename;
		unsigned current_line, current_column;
		
		clang_getPresumedLocation(location, &current_filename, 
					  &current_line, &current_column);
		
		CXType referenced_type = clang_getCursorType(referenced_cursor);
		bool is_ptr_type;

		switch (referenced_type.kind) {
			case CXType_Pointer:
			case CXType_ConstantArray:
			case CXType_IncompleteArray:
			case CXType_VariableArray:
				is_ptr_type = true;
				break;
			default:
				is_ptr_type = false;
				break;
		}

		if (closest_callexpr_offset < closest_binop_offset &&
		    closest_callexpr_offset < closest_arrsubexp_offset) {
			CXCursor callexpr_decl = clang_getCursorReferenced(closest_callexpr_ancestor);
			CXString callexpr_usr = clang_getCursorUSR(callexpr_decl);
			if (closest_unop_offset < closest_callexpr_offset &&
			    closest_unop_offset < closest_arrsubexp_offset) {
				enum CXUnaryOperatorKind unop_kind = 
					clang_getCursorUnaryOperatorKind(closest_unop_ancestor);
				if (unop_kind == CXUnaryOperator_AddrOf) {
					// Push as Escape
					size_t param_idx = get_param_idx(closest_callexpr_ancestor,
									 prong_priv->ancestry_stack);
					push_var_access(current_func->var_accesses,
							clang_getCString(decl_usr),
							clang_getCString(decl_name),
							clang_getCString(callexpr_usr),
							param_idx,
							current_line, current_column, 
							VarAccess_Escape, is_ptr_type);
				} else {
					// Push as Read
					size_t param_idx = get_param_idx(closest_callexpr_ancestor,
									 prong_priv->ancestry_stack);
					push_var_access(current_func->var_accesses,
							clang_getCString(decl_usr),
							clang_getCString(decl_name),
							clang_getCString(callexpr_usr),
							param_idx, current_line, current_column, 
							VarAccess_Read, is_ptr_type);
				}
			} else if (is_ptr_type) {
				// Push as Escape
				size_t param_idx = get_param_idx(closest_callexpr_ancestor,
								 prong_priv->ancestry_stack);
				push_var_access(current_func->var_accesses,
						clang_getCString(decl_usr),
						clang_getCString(decl_name),
						clang_getCString(callexpr_usr),
						param_idx, 
						current_line, current_column, 
						VarAccess_Escape, is_ptr_type);
				
			} else {
				// Go to this thingy 
				size_t param_idx = get_param_idx(closest_callexpr_ancestor,
								 prong_priv->ancestry_stack);
				push_var_access(current_func->var_accesses,
						clang_getCString(decl_usr),
						clang_getCString(decl_name),
						clang_getCString(callexpr_usr),
						param_idx, current_line, current_column, 
						VarAccess_Read, is_ptr_type);
			}
			clang_disposeString(callexpr_usr);
			goto visitor_recurse;
		}

		if (closest_arrsubexp_offset < closest_binop_offset && 
		    closest_arrsubexp_offset < closest_unop_offset) {
			// Check if our cursor is on the rhs or lhs of 
			// the ArraySubscriptExpr
			CXCursor arrsubexp_lhs = get_cursor_first_child(closest_arrsubexp_ancestor);
			if (cursors_are_equal(current_cursor, arrsubexp_lhs) || 
			    in_cursor_stack(prong_priv->ancestry_stack, arrsubexp_lhs)) {
				// Check if it's on lhs of binary operator
				if (closest_binop_offset != ANCESTOR_NOT_FOUND) {
					CXCursor binop_lhs = get_cursor_first_child(closest_binop_ancestor);
					if (cursors_are_equal(current_cursor, binop_lhs) ||
					    in_cursor_stack(prong_priv->ancestry_stack, binop_lhs)) {
						// Push as write
						push_var_access(current_func->var_accesses,
								clang_getCString(decl_usr),
								clang_getCString(decl_name),
								NULL, NO_IDX, 
								current_line, current_column, 
								VarAccess_PtrWrite, is_ptr_type);
					} else {
						// Push as read
						push_var_access(current_func->var_accesses,
								clang_getCString(decl_usr),
								clang_getCString(decl_name),
								NULL, NO_IDX,
								current_line, current_column, 
								VarAccess_PtrRead, is_ptr_type);
					}	
				}
			} else {
				// Push as read
				push_var_access(current_func->var_accesses,
						clang_getCString(decl_usr),
						clang_getCString(decl_name),
						NULL, NO_IDX,
						current_line, current_column, 
						VarAccess_Read, is_ptr_type);
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
					if (cursors_are_equal(current_cursor, binop_lhs) || 
					    in_cursor_stack(prong_priv->ancestry_stack, binop_lhs)) {
						if (cursors_are_equal(current_cursor, binop_lhs) ||
						    in_cursor_stack(prong_priv->ancestry_stack, 
									non_assign_binop_lhs)) {
							push_var_access(current_func->var_accesses,
									clang_getCString(decl_usr),
									clang_getCString(decl_name),
									NULL, NO_IDX,
									current_line, current_column, 
									VarAccess_PtrWrite, is_ptr_type);	
						} else {
							push_var_access(current_func->var_accesses,
									clang_getCString(decl_usr),
									clang_getCString(decl_name),
									NULL, NO_IDX,
									current_line, current_column, 
									VarAccess_Read, is_ptr_type);
						}
					} else {
						// Push as read if not
						push_var_access(current_func->var_accesses,
								clang_getCString(decl_usr),
								clang_getCString(decl_name),
								NULL, NO_IDX,
								current_line, current_column, 
								VarAccess_PtrRead, is_ptr_type);
					}
				} else {
					push_var_access(current_func->var_accesses,
							clang_getCString(decl_usr),
							clang_getCString(decl_name),
							NULL, NO_IDX,
							current_line, current_column, 
							VarAccess_PtrRead, is_ptr_type);
				}
			} else if (unop_kind == CXUnaryOperator_PreInc ||
				   unop_kind == CXUnaryOperator_PostInc ||
				   unop_kind == CXUnaryOperator_PreDec ||
				   unop_kind == CXUnaryOperator_PostDec) {
				// Definitely push as write
				push_var_access(current_func->var_accesses,
						clang_getCString(decl_usr),
						clang_getCString(decl_name),
						NULL, NO_IDX, 
						current_line, current_column, 
						VarAccess_Write, is_ptr_type);
			} else {
				// Push as read
				push_var_access(current_func->var_accesses,
						clang_getCString(decl_usr),
						clang_getCString(decl_name),
						NULL, NO_IDX, 
						current_line, current_column, 
						VarAccess_Read, is_ptr_type);
			}
			
		} else if (closest_binop_offset < closest_unop_offset && 
			   closest_binop_offset < closest_arrsubexp_offset) {
			// Just check if current cursor is on lhs or rhs
			CXCursor binop_lhs = get_cursor_first_child(closest_binop_ancestor);
			
			// Push as write if LHS
			if (cursors_are_equal(current_cursor, binop_lhs) ||
			    in_cursor_stack(prong_priv->ancestry_stack, binop_lhs)) {
				// Push as write
				push_var_access(current_func->var_accesses,
						clang_getCString(decl_usr),
						clang_getCString(decl_name),
						NULL, NO_IDX, 
						current_line, current_column, 
						VarAccess_Write, is_ptr_type);
			} else {
				// Push as read if not
				push_var_access(current_func->var_accesses,
						clang_getCString(decl_usr),
						clang_getCString(decl_name),
						NULL, NO_IDX, 
						current_line, current_column, 
						VarAccess_Read, is_ptr_type);
			}
			
		} else	{
			push_var_access(current_func->var_accesses,
					clang_getCString(decl_usr),
					clang_getCString(decl_name),
					NULL, NO_IDX, 
					current_line, current_column, 
					VarAccess_Read, is_ptr_type);

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
			CXCursor working_cursor = callee_decl;

			if (!clang_isCursorDefinition(working_cursor)) {
				working_cursor = find_callexpr_definition(callee_decl, prong_priv);
				if (clang_Cursor_isNull(working_cursor))
					working_cursor = callee_decl;
			}

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
				clang_Location_isInSystemHeader(callee_location),
				clang_isCursorDefinition(working_cursor));
			
			clang_disposeString(callee_usr);
			clang_disposeString(callee_name);
			
			FuncInfo *tail = func_info_array_tail(prong_priv->current_func->callees);
			if (tail->in_system_header || !tail->has_definition) {
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

/* Goes into esc_func_info var access list and 
 * overrides parameter variable access identities that 
 * correspond to the var_access argument passed 
 * to the escape function (by just replacing the USR) */
static void resolve_var_access_alias(FuncInfo *esc_func_info, VarAccess *var_access)
{
	if (func_info_is_null(esc_func_info)) {
		print_warn(
		"Tried to resolve variable access alias for null esc_func_info \
		perhaps not found?\n"
		);
		return;
	}

	CXCursor param_cursor = clang_Cursor_getArgument(esc_func_info->cursor, 
							 var_access->esc_param_idx);
	CXString param_usr = clang_getCursorUSR(param_cursor);
	const char *param_usr_str = clang_getCString(param_usr);

	if (esc_func_info->var_accesses) {
		for (size_t i = 0; i < esc_func_info->var_accesses->size; ++i) {
			VarAccess *working_va = &esc_func_info->var_accesses->data[i];
				
			if (strcmp(working_va->usr, param_usr_str) == 0) {
				// Override this var access with escaped variable identity
				// (just USR is sufficient for now)
				free(working_va->usr);
				working_va->usr = strdup(var_access->usr);
			}
		}
	}

	clang_disposeString(param_usr);
}

// This function was made with by deepseek
/* Generate an artificial USR that's distinct from the usual
 * USR format but unique enough to compare to libclang USRs */
static char *generate_artificial_param_usr(const char *name)
{
	const char *safe_name = name ? name : "";

	size_t name_len = strlen(safe_name);

	unsigned hash = 5381;
	for (size_t i = 0; i < name_len; ++i) {
		hash = ((hash << 5) + hash) + (unsigned char)safe_name[i];
	}

	// Format: p:<hash>:<namelen>:<name>
	size_t buffer_size = 64 + name_len;
	char *usr = (char*)malloc(buffer_size);
	if (!usr) return NULL;

	if (name_len > 0) {
		snprintf(usr, buffer_size, "p:%u:%zu:%s",
				hash, name_len, safe_name);
	} else {
		snprintf(usr, buffer_size, "p:null:unnamed");
	}

	return usr;
}

/* Maps all escaped variables and parameters to their
 * callee's variable accesses and nulls out unnecessary
 * VarAccess structs (like those of pointer type variables 
 * that never escape)
 * - definition of escaped variables in
 * types.h VarAccessType enum definition*/
void unwind_func_info(FuncInfo *func_info,
			struct prong_priv *client_data,
			DynamicAOS *parsed_func_call)
{
	if (func_info->in_system_header || !func_info->has_definition)
		return;

	VarAccessArr *var_accesses = func_info->var_accesses;
	// If we have a parsed function call, we're at the root function so
	// we "map" our inputted arguments to our "parameter" variable accesses
	// with an artificially generated "USR" 
	if (parsed_func_call != NULL) {
		for (size_t i = 1; i < parsed_func_call->count; ++i) {
			const char *param_usr = aos_string_at(func_info->params, i-1);
			if (var_accesses && param_usr) {
				for (size_t j = 0; j < var_accesses->size; ++j) {
					VarAccess *var_access = &var_accesses->data[j];

					if (strcmp(var_access->usr, param_usr) == 0) {
						const char *parsed_arg = aos_string_at(parsed_func_call, i);
						free(var_access->usr);
						var_access->usr = generate_artificial_param_usr(parsed_arg);
					}
				}

			}

		}
	}
	
	// Resolve var to param aliases for this FuncInfo
	if (func_info->callees) {
		for (size_t i = 0; i < var_accesses->size; ++i) {
			VarAccess *var_access = &var_accesses->data[i];
			if(var_access->type == VarAccess_Escape) {
				// Go into callee parameters and replace
				// var access USR, names etc.
				FuncInfo *esc_func_info = get_func_info_by_usr(func_info->callees,
									       var_access->esc_func_usr);
				if (esc_func_info && !esc_func_info->in_system_header)
					resolve_var_access_alias(esc_func_info, var_access);
			}
		}
	}

	// Null out irrelevant var accesses
	if (var_accesses != NULL) {
		for (size_t i = 0; i < var_accesses->size; ++i) {
			VarAccess *var_access = &var_accesses->data[i];

			if (/* var access usr is in locals but never escapes */
			    !var_access->is_ptr_type &&
			    var_access->type != VarAccess_Escape &&
			    aos_contains_string(func_info->locals, var_access->usr)) {
				var_access->type = VarAccess_Null;
			}

			if (/* var access usr is in params and is normal variable read/write */
			    (var_access->type == VarAccess_Read || var_access->type == VarAccess_Write) &&
			    aos_contains_string(func_info->params, var_access->usr)) {
				var_access->type = VarAccess_Null;
			}
		}

	}

	// Recurse
	if (func_info->callees && !func_info->in_system_header) {
		for (size_t i = 0; i < func_info->callees->size; ++i) {
			FuncInfo *working_func_info = &func_info->callees->data[i];
			unwind_func_info(working_func_info,
					 client_data, NULL);
		}
	}
}

/* Collect all VarAccess structs that weren't nullified
 * into the root FuncInfo's top level footprint collection
 * of all variable accesses at func_info->access_footprint */
void build_var_access_footprint(FuncInfo *func_info, VarAccessArr *access_footprint) 
{
	if (func_info->in_system_header || !func_info->has_definition)
		return;

	if (func_info->callees) {
		for (size_t i = 0; i < func_info->callees->size; ++i) {
			build_var_access_footprint(&func_info->callees->data[i],
						   access_footprint);
		}
	}

	for (size_t i = 0; i < func_info->var_accesses->size; ++i) {
		VarAccess *working_va = &func_info->var_accesses->data[i];
	
		if (working_va->type != VarAccess_Null) 
			push_access_copy(access_footprint, working_va); 
	}
}

static void print_call_trace(FuncInfo *func_info,
			     VarAccess *var_access, 
			     DynamicAOS *call_trace) 
{
	if (!call_trace || call_trace->count == 0) return;

	CXCursor func_cursor = func_info->cursor;
	CXTranslationUnit tu = clang_Cursor_getTranslationUnit(func_cursor);

	CXSourceLocation func_cursor_loc = clang_getCursorLocation(func_cursor);

	CXFile file;
	clang_getSpellingLocation(func_cursor_loc, &file, NULL, NULL, NULL);

	CXString file_name = clang_getFileName(file);

	// Print the call chain as a tree
	for (size_t i = 0; i < call_trace->count; ++i) {
		// Indentation for depth > 0
		if (i > 0) {
			// For each ancestor level, print spaces (or vertical pipes if you want)
			for (size_t d = 0; d < i - 1; ++d)
				printf("	");	  // 4 spaces per depth (no branching)
			printf("%s└──%s ", CLR_ARROW, CLR_RESET);
		}

		// Function name
		printf("%s%s%s", CLR_FUNC, call_trace->strings[i], CLR_RESET);

		// If this is the last function in the chain, append the access details on the same line
		if (i == call_trace->count - 1) {
			printf(" %s→%s %s%s, line: %d, col: %d%s  %s│%s ",
				   CLR_ARROW, CLR_RESET,
				   CLR_LOC, clang_getCString(file_name), var_access->line, 
				   var_access->column, CLR_RESET,
				   CLR_ARROW, CLR_RESET);

			// Access type
			switch (var_access->type) {
				case VarAccess_Read:	printf("%sREAD%s", CLR_READ, CLR_RESET); break;
				case VarAccess_Write:	printf("%sWRITE%s", CLR_WRITE, CLR_RESET); break;
				case VarAccess_PtrRead:	printf("%sREAD FROM ADDR%s", CLR_PTRREAD, CLR_RESET); break;
				case VarAccess_PtrWrite:printf("%sWRITE TO ADDR%s", CLR_PTRWRITE, CLR_RESET); break;
				case VarAccess_Escape:	printf("%sESCAPE%s", CLR_ESCAPE, CLR_RESET); break;
				default:		printf("?");
			}

			printf(" : %s%s%s", CLR_VAR, var_access->name, CLR_RESET);
			if (var_access->is_ptr_type)
				printf(" (ptr)");
			printf("\n");
		} else {
			printf("\n");
		}
	}
	print_source_line(clang_getLocation(tu, file, 
					var_access->line, var_access->column));
	clang_disposeString(file_name);
}

/* Goes in "func_info" graph recursively and tries to find
 * "var_access" struct while keeping track of it's "call_trace".
 * When it finds the function, it prints out the call trace. */
void trace_va_overlap(FuncInfo *func_info,
		      VarAccess *var_access,
		      DynamicAOS *call_trace)
{
	aos_push_string(call_trace, func_info->name);

	if (func_info->in_system_header || !func_info->has_definition)
		goto end_recursion;

	if (func_info->var_accesses) {
		for (size_t i = 0; i < func_info->var_accesses->size; ++i) {
			VarAccess *working_va = &func_info->var_accesses->data[i];
			if (equal_var_access_structs(var_access, working_va)) {
				print_call_trace(func_info, var_access, call_trace);
				break;
			}
		}
	}

	if (func_info->callees) {
		for (size_t i = 0; i < func_info->callees->size; ++i) {
			FuncInfo *working_func_info = &func_info->callees->data[i];
			trace_va_overlap(working_func_info, var_access, 
					 call_trace);
		}
	}

end_recursion:
	aos_pop_string(call_trace);
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
	prong_priv->clang_args = init_aos();

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

	if (prong_priv->clang_args)
		free_aos(prong_priv->clang_args);

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

// This function was also made with deepseek
/* Takes "input" in the format of "foo(arg1, arg2, ...)" and
 * pushes function name, argument names in order to external
 * DynamicAOS "out" in the format of ["foo", "arg1", "arg2", ...] */
error_t parse_func_call(const char *input, DynamicAOS *out) {
	if (!input || !out) return ERR_INVALID_ARG;
	
//	size_t len = strlen(input);
	
	// Find the opening paren
	const char *paren = strchr(input, '(');
	if (!paren) return ERR_SYNTAX;
	
	// Extract function name (everything before the paren)
	// Trim trailing whitespace
	const char *name_end = paren;
	while (name_end > input && isspace((unsigned char)*(name_end - 1))) {
	    name_end--;
	}
	
	size_t name_len = name_end - input;
	if (name_len == 0) return ERR_SYNTAX;
	
	char *func_name = strndup(input, name_len);
	if (!func_name) return ERR_OUT_OF_MEMORY;
	aos_push_string(out, func_name);
	free(func_name);
	
	// Find the closing paren
	const char *closing = strrchr(paren, ')');
	if (!closing) return ERR_SYNTAX;
	
	// Extract arguments between parens
	const char *args_start = paren + 1;
	const char *args_end = closing;
	
	// Parse comma-separated arguments
	const char *cursor = args_start;
	while (cursor < args_end) {
	    // Skip leading whitespace
	    while (cursor < args_end && isspace((unsigned char)*cursor)) {
	        cursor++;
	    }
	    if (cursor >= args_end) break;
	    
	    // Find end of this argument (comma or closing paren)
	    const char *arg_start = cursor;
	    const char *arg_end = cursor;
	    int depth = 0;  // Track nested parens for function pointer args
	    
	    while (cursor < args_end) {
	        if (*cursor == '(') depth++;
	        else if (*cursor == ')') depth--;
	        else if (*cursor == ',' && depth == 0) break;
	        cursor++;
	    }
	    arg_end = cursor;
	    
	    // Trim trailing whitespace from argument
	    while (arg_end > arg_start && isspace((unsigned char)*(arg_end - 1))) {
	        arg_end--;
	    }
	    
	    if (arg_end > arg_start) {
	        char *arg = strndup(arg_start, arg_end - arg_start);
	        if (!arg) return ERR_OUT_OF_MEMORY;
	        aos_push_string(out, arg);
	        free(arg);
	    }
	    
	    // Skip the comma
	    if (cursor < args_end && *cursor == ',') {
	        cursor++;
	    }
	}
	
	return ERR_OK;
}

/* Turns semicolon-separated strings into a 
 * dynamic string array DynamicAOS */
static void split_semicolon_list(char *str_in, DynamicAOS *dyn_aos)
{
	char *files_str_array = strdup(str_in);
	char *temp = files_str_array;
	size_t num_files = 1;
	size_t offset = 0;

	while (temp[offset] != '\0') {
		if (temp[offset] == ';') {
			temp[offset] = '\0';
			num_files++;
		}
		++offset;
	}

	for (size_t n = 0; n < num_files; ++n) {
		// Skip leading whitespace
		while (*temp == ' ' || *temp == '\t') temp++;
		if (*temp != '\0') {
			aos_push_string(dyn_aos, temp);
		}
		temp += strlen(temp) + 1;
	}

	free(files_str_array);
}

/* Turns semicolon-separated strings into a 
 * dynamic string array DynamicAOS */
static void split_comma_list(char *str_in, DynamicAOS *dyn_aos)
{
	char *files_str_array = strdup(str_in);
	char *temp = files_str_array;
	size_t num_files = 1;
	size_t offset = 0;

	while (temp[offset] != '\0') {
		if (temp[offset] == ',') {
			temp[offset] = '\0';
			num_files++;
		}
		++offset;
	}

	for (size_t n = 0; n < num_files; ++n) {
		// Skip leading whitespace
		while (*temp == ' ' || *temp == '\t') temp++;
		if (*temp != '\0') {
			aos_push_string(dyn_aos, temp);
		}
		temp += strlen(temp) + 1;
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

	for (int i = 1; i < argc; ++i) {
		cmd_arg	= argv[i];

		if (strncmp(cmd_arg, "--files=", FILES_ARG_STRLEN) == 0) {
			split_comma_list(cmd_arg+FILES_ARG_STRLEN, prong_priv->file_names);
		} else if (strncmp(cmd_arg, "--functions=", FUNCS_ARG_STRLEN) == 0) {
			split_semicolon_list(cmd_arg+FUNCS_ARG_STRLEN, prong_priv->func_names);
		} else if (strcmp(cmd_arg, "--verbose") == 0) {
			arg_verbose = true;
		} else if (strcmp(cmd_arg, "--help") == 0) {
			arg_help = true;
		} else {
			aos_push_string(prong_priv->clang_args, cmd_arg);
		}
	}

	if (prong_priv->func_names->count == 0 ||
		prong_priv->file_names->count == 0)
		return ERR_INVALID_ARG;

	return 0;
}

void print_usage(const char *prog_name) {
    printf("Usage: %s --files=\"file1,file2,...\" --functions=\"foo(arg1, arg2);bar(arg1, arg2);...\" [OPT ARGS] [clang ARGS]\n", prog_name);
    printf("\n");
    printf("Required arguments:\n");
    printf("  --files=LIST        Semicolon-separated list of input source files\n");
    printf("  --functions=LIST    Semicolon-separated list of function calls\n");
    printf("\n");
    printf("Function call format:\n");
    printf("  foo(arg1, arg2, shared_arg)\n\n");
    printf("  bar(shared_arg, arg3, arg4)\n");
    printf("\n");
    printf("Optional arguments:\n");
    printf("  --verbose           Enable verbose output\n");
    printf("  --help              Show this help message and exit\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s --files=\"main.c,util.c\" --functions=\"init();run(arg)\"\n", prog_name);
    printf("  %s --files=\"a.c,b.c,c.c\" --functions=\"foo(arg);bar(arg)\" --verbose\n", prog_name);
}


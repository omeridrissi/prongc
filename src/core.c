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

static bool operator_is_assignment(CXCursor cursor) {
	enum CXBinaryOperatorKind opcode = clang_getCursorBinaryOperatorKind(cursor);
	print_debug("binop opcode: %d\n", opcode);
	/* If it's any type of assignment */
	switch (opcode) {
		case CXBinaryOperator_Assign:       // =
        	case CXBinaryOperator_AddAssign:    // +=
        	case CXBinaryOperator_SubAssign:    // -=
        	case CXBinaryOperator_MulAssign:    // *=
        	case CXBinaryOperator_DivAssign:    // /=
        	case CXBinaryOperator_RemAssign:    // %=
        	case CXBinaryOperator_ShlAssign:    // <<=
        	case CXBinaryOperator_ShrAssign:    // >>=
        	case CXBinaryOperator_AndAssign:    // &=
        	case CXBinaryOperator_XorAssign:    // ^=
        	case CXBinaryOperator_OrAssign:     // |=
			// This is an assignment operation, now check LHS position
			return true;
			break;
        	default:
			return false;  // Not an assignment operator
	}
}

static enum CXChildVisitResult get_first_child_visitor(CXCursor cursor,
						     CXCursor parent,
						     CXClientData data)
{
	(void)parent;
	CXCursor *result = (CXCursor *)data;
	*result = cursor;
	return CXChildVisit_Break;
}
CXCursor get_cursor_first_child(CXCursor cursor)
{
	CXCursor result;
	clang_visitChildren(cursor, get_first_child_visitor, &result);
	return result;
}

static enum CXChildVisitResult get_first_usr_visitor(CXCursor cursor,
						     CXCursor parent,
						     CXClientData data)
{
	(void)parent;
	char **result = (char **)data;
	CXCursor cursor_referenced = clang_getCursorReferenced(cursor);
	CXString cxstring = clang_getCursorUSR(cursor_referenced);
	*result = strdup(clang_getCString(cxstring));
	clang_disposeString(cxstring);
	return CXChildVisit_Break;
}
char *get_cursor_first_child_usr(CXCursor cursor)
{
	char *result;
	clang_visitChildren(cursor, get_first_usr_visitor, &result);
	return result;
}

/* Goes into the cursor's parents as far as
 * possible and returns a parent that matches 
 * the specified kind */
static CXCursor get_ancestor_of_kind(CXCursor cursor, enum CXCursorKind kind) 
{
	CXCursor parent = clang_getCursorLexicalParent(cursor);
	print_debug("lexical parent kind: %d\n", clang_getCursorKind(parent));
	while (!clang_Cursor_isNull(parent)) { 
		if (clang_getCursorKind(parent) == kind)
			return parent;
		parent = clang_getCursorSemanticParent(parent);
	}
	return parent;
}

static CXCursor get_assignment_ancestor(CXCursor cursor) 
{
	CXCursor binop_assignment_ancestor = get_ancestor_of_kind(cursor, 
								  CXCursor_BinaryOperator);
	if (clang_Cursor_isNull(binop_assignment_ancestor))
		print_debug("binop_assignment_ancestor is null on first return \n");
	while (!clang_Cursor_isNull(binop_assignment_ancestor) &&
		!operator_is_assignment(binop_assignment_ancestor)) {
		binop_assignment_ancestor = 
			get_ancestor_of_kind(binop_assignment_ancestor,
					     CXCursor_BinaryOperator);
	}

	return binop_assignment_ancestor;
}


/* Visitor function for looping through 
 * the children of the compound statement cursor 
 * relating to the function we've filtered 
 * out from the AST */
static enum CXChildVisitResult prong_visitor_walk_func(CXCursor current_cursor,
						[[maybe_unused]]CXCursor parent_cursor,
						CXClientData client_data)
{
	struct prong_priv *prong_priv = (struct prong_priv*)client_data;

	prong_priv->recursion_depth++;

	enum CXCursorKind current_cursor_kind = clang_getCursorKind(current_cursor);
	enum CXLinkageKind current_cursor_linkage = clang_getCursorLinkage(current_cursor);

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

	if (current_cursor_kind == CXCursor_BinaryOperator &&
	    operator_is_assignment(current_cursor)) {
		push_cursor(prong_priv->ancestor_registry, &current_cursor);
	}

	if (current_cursor_kind == CXCursor_DeclRefExpr) {
		FuncInfo *current_func = prong_priv->current_func;
		if (!current_func->var_accesses)
			current_func->var_accesses = init_var_access_array();
		
		print_debug("Found CXCursor_DeclRefExpr\n");

		//CXCursor parent_cursor = clang_getCursorLexicalParent(current_cursor);
		/* Check if this is an assignment */
		enum CXCursorKind refexpr_parent_kind = clang_getCursorKind(parent_cursor);
		print_debug("Immediate parent kind: %d\n", refexpr_parent_kind);

		CXCursor referenced_cursor = clang_getCursorReferenced(current_cursor);
		CXString current_usr = clang_getCursorUSR(referenced_cursor);
		CXString current_name = clang_getCursorDisplayName(referenced_cursor);
		
		CXSourceLocation location = clang_getCursorLocation(current_cursor);
		CXString current_filename;
		unsigned current_line, current_column;
		
		clang_getPresumedLocation(location, &current_filename, 
					  &current_line, &current_column);
		
		CXCursor binop_assignment_ancestor = get_assignment_ancestor(current_cursor);
		
		goto visitor_recurse;
		if (refexpr_parent_kind == CXCursor_BinaryOperator &&
		    operator_is_assignment(parent_cursor)) {
			print_debug("And it's a binary operator!\n");
			char *first_child_usr = get_cursor_first_child_usr(parent_cursor);
			if (strcmp(clang_getCString(current_usr), first_child_usr) == 0) {
				// Push as write
				push_var_access(current_func->var_accesses,
						clang_getCString(current_usr),
						clang_getCString(current_name),
						NULL, current_line, current_column, VarAccess_Write);
			} else {
				// Push as read
				push_var_access(current_func->var_accesses,
						clang_getCString(current_usr),
						clang_getCString(current_name),
						NULL, current_line, current_column, VarAccess_Read);
			}
			free(first_child_usr);
		} else if (refexpr_parent_kind == CXCursor_UnaryOperator) {
			print_debug("And it's a unary operator!\n");
			enum CXUnaryOperatorKind unop_kind =
				clang_getCursorUnaryOperatorKind(parent_cursor);
			if (unop_kind == CXUnaryOperator_Deref) {
				CXCursor super_parent_lhs = get_cursor_first_child(binop_assignment_ancestor);

				// Push as write if LHS
				if (in_cursor_branch(super_parent_lhs, current_cursor)) {
					// Push as write
					push_var_access(current_func->var_accesses,
							clang_getCString(current_usr),
							clang_getCString(current_name),
							NULL, current_line, current_column, VarAccess_Write);
				} else {
					// Push as read if not
					push_var_access(current_func->var_accesses,
							clang_getCString(current_usr),
							clang_getCString(current_name),
							NULL, current_line, current_column, VarAccess_Read);
				}


			} else if (unop_kind == CXUnaryOperator_PreInc ||
				   unop_kind == CXUnaryOperator_PostInc ||
				   unop_kind == CXUnaryOperator_PreDec ||
				   unop_kind == CXUnaryOperator_PostDec) {
				// Definitely push as write
				push_var_access(current_func->var_accesses,
						clang_getCString(current_usr),
						clang_getCString(current_name),
						NULL, current_line, current_column, VarAccess_Write);
			} else {
				// Push as read
				push_var_access(current_func->var_accesses,
						clang_getCString(current_usr),
						clang_getCString(current_name),
						NULL, current_line, current_column, VarAccess_Read);
			}
		} 
		
		clang_disposeString(current_usr);
		clang_disposeString(current_name);
		clang_disposeString(current_filename);
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
				goto visitor_recurse;
			}
			process_func_info(func_info_array_tail(
						prong_priv->current_func->callees
					  ),
					  client_data);

		}
		goto visitor_recurse;	
	}

visitor_recurse:
	prong_priv->recursion_depth--;

	return CXChildVisit_Recurse;
}

/* Allocates an array of CXTranslationUnit structs */
CXTranslationUnit *alloc_tu_array(int length) 
{
	CXTranslationUnit *tus;
	tus = (CXTranslationUnit*)malloc(sizeof(CXTranslationUnit) * length);

	return tus;
}

void free_tu_array(CXTranslationUnit *tu_array, int length)
{
	for (int i = 0; i < length; ++i) {
		clang_disposeTranslationUnit(tu_array[i]);
	}
}

/* Checks if a cursor struct is uninitialized */
bool cursor_empty(CXCursor *cursor)
{
	if (cursor->data[0] == NULL &&
		cursor->data[1] == NULL &&
		cursor->data[2] == NULL &&
		cursor->xdata == 0 && 
		cursor->kind == 0)
		return false;
	else
		return true;
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

/* Recursively visits and processes FuncInfo struct
 * process_func_info->clang_visitChildren->
 * prong_visitor_walk_func->process_func_info */
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

	prong_priv->ancestor_registry = init_cursor_array();

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

	if (prong_priv->ancestor_registry)
		free_cursor_array(prong_priv->ancestor_registry);
	
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
 * and save it in state (prong_priv/client_data struct) */
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


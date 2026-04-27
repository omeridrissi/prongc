#include "core.h"
#include "dyn_aos.h"
#include "func_info.h"

/* Visitor function for matching our specified 
 * function in args to our desired function 
 * definition in the AST */
static enum CXChildVisitResult prong_visitor_walk_ast(CXCursor current_cursor, 
					    CXCursor parent_cursor, 
					    CXClientData client_data)
{
	struct prong_priv *prong_priv = (struct prong_priv *)client_data;

	CXString parent_display_name = clang_getCursorDisplayName(parent_cursor);
	
	enum CXCursorKind current_cursor_kind = clang_getCursorKind(current_cursor);
	enum CXCursorKind parent_cursor_kind = clang_getCursorKind(parent_cursor);

	const char *elem_name = clang_getCString(parent_display_name);

	/* Collects function declaration cursors */
	if ((current_cursor_kind == CXCursor_CompoundStmt) &&
	    (parent_cursor_kind == CXCursor_FunctionDecl)) {
		for (int i = 0; i < prong_priv->num_funcs; ++i) {
			int func_name_len = strlen(prong_priv->func_names[i]);
			int elem_name_len = strlen(elem_name);

			/* Match a specific function body definition */
			if ((func_name_len == elem_name_len-2) &&
				(strncmp(elem_name, 
					prong_priv->func_names[i],
					elem_name_len-2) == 0)) {
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
						clang_getCString(parent_display_name));
				
				clang_disposeString(cursor_usr);
			}
		}
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

	/* If this is true, we have ourselves a
	 * local variable declaration */
	if (current_cursor_kind == CXCursor_VarDecl &&
	    current_cursor_linkage == CXLinkage_NoLinkage) {
		CXString current_cursor_usr = clang_getCursorUSR(current_cursor);

		const char *current_usr_string = clang_getCString(current_cursor_usr);
		aos_push_string(prong_priv->current_func->locals, current_usr_string);

		clang_disposeString(current_cursor_usr);

	}
	
	/* If this is true, we have ourselves a
	 * parameter declaration */
	if (current_cursor_kind == CXCursor_ParmDecl && 
	    current_cursor_linkage == CXLinkage_NoLinkage) {
		CXString current_cursor_usr = clang_getCursorUSR(current_cursor);

		const char *current_usr_string = clang_getCString(current_cursor_usr);
		aos_push_string(prong_priv->current_func->params, current_usr_string);

		clang_disposeString(current_cursor_usr);

	}

	/* If we have a function call, push that function's
	 * definition onto the list of callees in our current
	 * FuncInfo struct, then recursively process that 
	 * function */
	if (current_cursor_kind == CXCursor_CallExpr) {
		CXCursor callee_decl = clang_getCursorReferenced(current_cursor);

		if (!clang_Cursor_isNull(callee_decl)) {
			CXCursor callee_def = clang_getCursorDefinition(callee_decl);
			if (!clang_Cursor_isNull(callee_def)) {
				CXString callee_usr = clang_getCursorUSR(callee_def);
				CXString callee_name = clang_getCursorDisplayName(callee_def);
			
				// Checks if callee array is initialized
				if (!prong_priv->current_func->callees)
					prong_priv->current_func->callees = 
						init_func_info_array();

				push_func_info(
					prong_priv->current_func->callees,
					&callee_def,
					clang_getCString(callee_usr),
					clang_getCString(callee_name));
				
				clang_disposeString(callee_usr);
				clang_disposeString(callee_name);

				process_func_info(func_info_array_tail(
							prong_priv->current_func->callees
						  ),
						  client_data);
			}
		}
	}

	return CXChildVisit_Continue;
}

/* Allocates an array of CXTranslationUnit structs */
CXTranslationUnit *alloc_tu_array(int length) 
{
	CXTranslationUnit *tus;
	tus = (CXTranslationUnit*)malloc(sizeof(CXTranslationUnit) * length);

	return tus;
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

	prong_priv->global_usrs = init_aos();

	prong_priv->funcs = init_func_info_array();

	return prong_priv;

exit:
	return NULL;
}

/* Frees state (prong_priv) and it's allocated fields */
void prong_free_priv(struct prong_priv *prong_priv) 
{
	char *string_copy_ptr = *prong_priv->func_names;
	if (string_copy_ptr)
		free(string_copy_ptr);

	string_copy_ptr = *prong_priv->file_names;
	if (string_copy_ptr)
		free(string_copy_ptr);

	if (prong_priv->func_names)
		free(prong_priv->func_names);
	if (prong_priv->file_names)
		free(prong_priv->file_names);

	if (prong_priv->global_usrs)
		free_aos(prong_priv->global_usrs);

	if (prong_priv->funcs) {
		free_func_info_array(prong_priv->funcs);
	}
	
	if (prong_priv)
		free(prong_priv);
	
}

/* Gets root cursor and visit it's children with
 * prong_visitor_walk_ast() function*/
void process_tu_array(CXTranslationUnit *tu_array, 
		      int length, 
		      struct prong_priv *client_data) 
{
	for (int i = 0; i < length; ++i) {
		CXCursor cursor = clang_getTranslationUnitCursor(tu_array[i]); 

		/* Find our desired function declaration cursors and push them to
		 * our cursor list inside of client_data */
		clang_visitChildren(cursor, 
				    prong_visitor_walk_ast, 
				    (void*)client_data);
	}

}

void process_func_info(FuncInfo *func_info,
			struct prong_priv *client_data)
{
	client_data->current_func = func_info;
	
	clang_visitChildren(*func_info->cursor,
			    prong_visitor_walk_func,
			    (void*)client_data);
}

#define MAX_NUM_SPLIT_STRS 24

/* Allocates a copy of the string containing our file names
 * that are comma separated and replaces the commas with null
 * characters, forming a tightly packed array of C strings.
 * Then returns an array of pointers, each element pointing to 
 * every file name in that string */
static char **split_args_comma(char *str_in, int *num_strs) {
	char *str_in_cpy = strdup(str_in);

	char *token;
	char *saveptr;

	char **result = (char**)malloc(sizeof(char*)*MAX_NUM_SPLIT_STRS);

	token = strtok_r(str_in_cpy, ",", &saveptr);

	*num_strs = 0;
	while (token != NULL) {
		result[*num_strs] = token;
		token = strtok_r(NULL, ",", &saveptr);
		*num_strs += 1;
		if (*num_strs > MAX_NUM_SPLIT_STRS)
			break;
	}

	return result;

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
			int num_strs;
			char **split_file_names = split_args_comma(cmd_arg+FILES_ARG_STRLEN, 
								   &num_strs);
			prong_priv->file_names = split_file_names;
			prong_priv->num_files = num_strs;
		} else if (strncmp(cmd_arg, "--functions=", FUNCS_ARG_STRLEN) == 0) {
			int num_strs;
			char **split_func_names = split_args_comma(cmd_arg+FUNCS_ARG_STRLEN,
								   &num_strs);
			prong_priv->func_names = split_func_names;
			prong_priv->num_funcs = num_strs;
		} else if (strcmp(cmd_arg, "--verbose") == 0) {
			arg_verbose = true;
		} else if (strcmp(cmd_arg, "--help") == 0) {
			arg_help = true;
		} else {
			continue;
		}
	}

	if (!prong_priv->file_names || !prong_priv->func_names ||
		prong_priv->num_files == 0 || prong_priv->num_funcs == 0)
		return ERR_INVALID_ARG;

	return 0;
}

void print_usage(const char *prog_name) {
    printf("Usage: %s --files=<file1,file2,...> --functions=<func1,func2,...> [OPTIONS]\n", prog_name);
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

/* This function shouldn't be used in final commits. 
 * Ideally, it should only be when debugging locally */
void print_debug(const char *format, ...)
{
	va_list args;
	va_start(args, format);

	printf("[dbg] ");
	vprintf(format, args);

	va_end(args);
}

/* This function should be used when printing information
 * that otherwise wouldn't be printed if the '--verbose' option 
 * isn't selected in the command line arguments */
void print_verbose(const char *format, ...)
{
	va_list args;
	va_start(args, format);

	printf("[ver] ");
	vprintf(format, args);

	va_end(args);
}

/* This should be used for printing fatal errors that 
 * result in prongc exitting */
void print_error(const char *format, ...)
{
	va_list args;
	va_start(args, format);

	printf("\033[37;41m[err]\033[0m ");
	vprintf(format, args);

	va_end(args);
}


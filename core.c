#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core.h"

static enum CXChildVisitResult prong_visitor(CXCursor current_cursor, 
					    CXCursor parent_cursor, 
					    CXClientData client_data)
{
	struct prong_priv *prong_priv = client_data;

	CXString parent_display_name = clang_getCursorDisplayName(parent_cursor);
	
//	CXType current_cursor_type = clang_getCursorType(current_cursor);
	enum CXCursorKind current_cursor_kind = clang_getCursorKind(current_cursor);
	enum CXCursorKind parent_cursor_kind = clang_getCursorKind(parent_cursor);

	char *elem_name = clang_getCString(parent_display_name);

	for (int i = 0; i < prong_priv->num_funcs; ++i) {
		int func_name_len = strlen(prong_priv->func_names[i]);
		int elem_name_len = strlen(elem_name);

		if (
		    (current_cursor_kind == CXCursor_CompoundStmt) &&
		    (parent_cursor_kind == CXCursor_FunctionDecl) &&
		    (func_name_len == elem_name_len-2) &&
		    (strncmp(elem_name, prong_priv->func_names[i], elem_name_len-2) == 0)
		    ) {
			printf("Visiting element %s\n", clang_getCString(parent_display_name));
			printf("	kind: %d\n", clang_getCursorKind(current_cursor));
		}
	}
	
	clang_disposeString(parent_display_name);
	
	return CXChildVisit_Recurse;
}

CXTranslationUnit *alloc_tu_array(int length) 
{
	CXTranslationUnit *tus;
	tus = (CXTranslationUnit*)malloc(sizeof(CXTranslationUnit) * length);

	return tus;
}

CXCursor *alloc_cursor_array(int length)
{
	CXCursor *cursors;
	cursors = (CXCursor*)malloc(sizeof(CXCursor) * length);

	return cursors;
}

struct prong_priv *prong_init_priv(char **argv, int funcs_pos, int num_funcs) 
{
	struct prong_priv *prong_priv;

	prong_priv = malloc(sizeof(*prong_priv));
	if (!prong_priv) 
		goto exit;

	memset(prong_priv, 0, sizeof(*prong_priv));

	prong_priv->func_names = malloc(sizeof(char*)*num_funcs);
	if (!prong_priv->func_names)
		goto free_priv;

	for (int i = 0; i < num_funcs; ++i) {
		prong_priv->func_names[i] = argv[funcs_pos+i];
	}
	prong_priv->num_funcs = num_funcs;

	prong_priv->cursors = alloc_cursor_array(DEFAULT_NUM_CURSORS);
	if (!prong_priv->cursors)
		goto free_func_names;
	prong_priv->num_cursors = DEFAULT_NUM_CURSORS;

free_func_names:
	free(prong_priv->func_names);

free_priv:
	free(prong_priv);

exit:
	return prong_priv;
}

void prong_free_priv(struct prong_priv *prong_priv) 
{
	free(prong_priv->func_names);
	free(prong_priv->cursors);
	free(prong_priv);
}

void process_tu_array(CXTranslationUnit *tu_array, int length, void *client_data) 
{
	for (int i = 0; i < length; ++i) {
		CXCursor cursor = clang_getTranslationUnitCursor(tu_array[i]); // Obtain cursor at root of translation unit

		int root_kind = clang_getCursorKind(cursor);
		printf("Root cursor kind: %d\n", root_kind);

		clang_visitChildren(cursor, prong_visitor, client_data);
	}

}

void process_args(int argc, char **argv, 
		  int *files_idx, int *funcs_idx,
		  int *num_files, int *num_funcs)
{
	int i;
	char *cmd_arg;

	for (i = argc-1; i > 0; --i) {
		cmd_arg	= argv[i];

		if (strcmp(cmd_arg, "-f") == 0) {
			if (i == argc-1 || *funcs_idx == i)
				return;
			else
				*files_idx = i+1;
		} else if (strcmp(cmd_arg, "-b") == 0) {
			if (i == argc-1 || *files_idx == i)
				return;
			else
				*funcs_idx = i+1;
		} else {
			continue;
		}
	}

	if (*files_idx < *funcs_idx) {
		*num_files = *funcs_idx - *files_idx - 1;
		*num_funcs = argc - *num_files - 3;
	} else {
		*num_funcs = *files_idx - *funcs_idx - 1;
		*num_files = argc - *num_funcs - 3;
	}
}

void print_usage(void) 
{
	printf("Usage: \n");
	printf("	prongc -f c-file1 c-file2 ... c-fileN -b func1() func2() ... funcN()\n");
}

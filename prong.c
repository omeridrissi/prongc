#include <stdio.h>
#include <stdlib.h>
#include <clang-c/Index.h>

#include "types.h"
#include "core.h"
#include "dyn_aos.h"

bool arg_verbose = false;
bool arg_help = false;

int main(int argc, char **argv) 
{
	struct prong_priv *client_data;
	error_t ret = ERR_OK;

	if (argc < 2) {
		print_usage(argv[0]);
		ret = ERR_INVALID_ARG;
		goto exit;
	}
	
	client_data = prong_init_priv();
	if (!client_data) {
		ret = ERR_OUT_OF_MEMORY;
		goto exit;
	}

	// Initializes some fields in client_data
	ret = process_args(argc, argv, client_data);
	if (ret) {
		print_usage(argv[0]);
		goto free_priv;
	}

	if (arg_help) {
		print_usage(argv[0]);
		goto free_priv;
	}

	print_debug("number of functions: %d\n", client_data->num_funcs);
	print_debug("number of files: %d\n", client_data->num_files);

	for (int i = 0; i < client_data->num_funcs; ++i) {
		print_debug("client_data->func_names[%d] = %s\n", 
				i, client_data->func_names[i]);
	}
	for (int i = 0; i < client_data->num_files; ++i) {
		print_debug("client_data->file_names[%d] = %s\n", 
				i, client_data->file_names[i]);
	}
	// Initialize cursor array in client_data
	client_data->cursors = alloc_cursor_array(client_data->num_cursors);
	if (!client_data->cursors) {
		ret = ERR_OUT_OF_MEMORY;
		goto free_priv;
	}
	memset(client_data->cursors, '\0', sizeof(CXCursor)*client_data->num_cursors);

	CXIndex index = clang_createIndex(0, 0);

	CXTranslationUnit *tu_array = alloc_tu_array(client_data->num_files);
	if (!tu_array) {
		ret = ERR_OUT_OF_MEMORY;
		goto free_cursors;
	}

	for (int i = 0; i < client_data->num_files; ++i) {
		CXTranslationUnit unit = clang_parseTranslationUnit(
			index,
			client_data->file_names[i], NULL, 0,
			NULL, 0,
			CXTranslationUnit_None
		);

		if (!unit) {
			print_error("Unable to parse translation unit. Quitting.\n");
			ret = ERR_NOT_FOUND; // it didn't find the file or whatever
			goto free_tu_array;
		}

		tu_array[i] = unit;
	}

	process_tu_array(tu_array, client_data->num_files, client_data);

	if (client_data->num_cursors_filled < client_data->num_cursors) {
		print_error("Couldn't find one of the functions \
				while walking AST. quitting...\n");
		ret = ERR_NOT_FOUND;
		goto free_tu_array;
	}

	print_debug("Looping through %d cursors\n", client_data->num_cursors);
	for (int i = 0; i < client_data->num_cursors; ++i) {
		CXCursor parent_cursor = client_data->cursors[i];
		CXString parent_display_name = clang_getCursorDisplayName(parent_cursor);
		if (arg_verbose) {
			print_verbose("Visiting element %s\n", 
					clang_getCString(parent_display_name));
			print_verbose("	kind: %d\n", 
					clang_getCursorKind(parent_cursor));

			CXString cursor_usr = clang_getCursorUSR(parent_cursor);
			print_verbose("	unified symbol representation: %s\n", 
					clang_getCString(cursor_usr));

			clang_disposeString(cursor_usr);
		}
		
		clang_disposeString(parent_display_name);
	}

free_tu_array:
	free(tu_array);
	
free_cursors:
	free(client_data->cursors);

free_priv:
	prong_free_priv(client_data);

exit:
	return ret;
}

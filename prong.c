#include <stdio.h>
#include <stdlib.h>
#include <clang-c/Index.h>

#include "types.h"
#include "core.h"
#include "dyn_aos.h"
#include "func_info.h"

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

	print_debug("Number of functio names = %d\n", client_data->func_names->count);
	print_debug("Function names:	");
	aos_print_strings(client_data->func_names);
	printf("\n");
	print_debug("Number of file names = %d\n", client_data->file_names->count);
	print_debug("File names: \n");
	aos_print_strings(client_data->file_names);
	printf("\n");

	CXIndex index = clang_createIndex(0, 0);

	CXTranslationUnit *tu_array = alloc_tu_array(client_data->file_names->count);
	if (!tu_array) {
		ret = ERR_OUT_OF_MEMORY;
		goto free_priv;
	}

	for (size_t i = 0; i < client_data->file_names->count; ++i) {
		tu_array[i] = clang_parseTranslationUnit(
			index,
			aos_string_at(client_data->file_names, i), NULL, 0,
			NULL, 0,
			CXTranslationUnit_None
		);

		if (!tu_array[i]) {
			print_error("Unable to parse translation unit. Quitting.\n");
			ret = ERR_TU; // it didn't find the file or whatever
			goto free_tu_array;
		}
	}

	process_tu_array(tu_array, client_data);
	
	if (client_data->funcs->size != client_data->func_names->count) {
		print_error("Could not find some functions you were looking for.\n");
		ret = ERR_NOT_FOUND;
		goto free_tu_array;
	}

	print_debug("number of FuncInfo structs: %zu\n", client_data->funcs->size);

	//for (size_t i = 0; i < client_data->funcs->size; ++i) {
	//	print_func_info(client_data->funcs->data+i, 1);
	//}

free_tu_array:
	free(tu_array);

free_priv:
	prong_free_priv(client_data);

exit:
	return ret;
}

#include <stdio.h>
#include <stdlib.h>
#include <clang-c/Index.h>

#include "types.h"
#include "core.h"
#include "dyn_aos.h"
#include "func_info.h"
#include "cursor_arr.h"
#include "var_access.h"
#include "log.h"

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

	if (client_data->func_names->count < 2) {
		print_error("Need multiple functions for variable overlap tracing\n");
		print_usage(argv[0]);
		goto free_priv;
	}

	if (arg_help) {
		print_usage(argv[0]);
		goto free_priv;
	}

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

		size_t num_diagnostics = clang_getNumDiagnostics(tu_array[i]);
		bool has_error = false;
		
		if (num_diagnostics == 0)
			continue;

		for (size_t j = 0; j < num_diagnostics; ++j) {
			CXDiagnostic diag = clang_getDiagnostic(tu_array[i], j);
			enum CXDiagnosticSeverity diag_severity = 
					clang_getDiagnosticSeverity(diag);
			CXString formatted;

			switch (diag_severity) {
				case CXDiagnostic_Error:
				case CXDiagnostic_Fatal:
					has_error = true;
					formatted = clang_formatDiagnostic(diag, 
							clang_defaultDiagnosticDisplayOptions());
					print_error("Error: \n\t%s\n", clang_getCString(formatted));
					clang_disposeString(formatted);
					break;
				case CXDiagnostic_Warning:
					formatted = clang_formatDiagnostic(diag, 
							clang_defaultDiagnosticDisplayOptions());
					print_error("Warning: \n\t%s\n", clang_getCString(formatted));
					clang_disposeString(formatted);
					break;
				default:
			}
			clang_disposeDiagnostic(diag);
		}

		if (has_error) {
			print_error("Errors detected parsing TU %d\n", i);
			ret = ERR_SYNTAX;
			goto free_tu_array;
		}
	}
	
	process_tu_array(tu_array, client_data);

	if (client_data->funcs->size != client_data->func_names->count) {
		print_error("Could not find some functions you were looking for.\n");
		ret = ERR_NOT_FOUND;
		goto free_tu_array;
	}

	for (size_t i = 0; i < client_data->funcs->size; ++i) {
		process_func_info(client_data->funcs->data+i, client_data);
	}
	
	reset_aos(&client_data->touched_func_usrs);

	// Done with this stage, probably won't need this anymore
	//if (arg_verbose) {
	//	print_verbose("showing raw state...\n");

	//	print_verbose("global variable USRs: ");
	//	aos_print_strings(client_data->global_usrs);
	//	printf("\n");

	//	print_func_info_array(client_data->funcs, 0);
	//}

	for (size_t i = 0; i < client_data->funcs->size; ++i) {
		const char *func_call = aos_string_at(client_data->func_names, i);
		DynamicAOS *parsed_func_call = init_aos();

		parse_func_call(func_call, parsed_func_call);
		
		unwind_func_info(&client_data->funcs->data[i], client_data, parsed_func_call);

		free_aos(parsed_func_call);
	}

	if (arg_verbose) {
		print_verbose("showing unwinded state...\n");
		print_verbose("global variable USRs: ");
		aos_print_strings(client_data->global_usrs);
		printf("\n");

		print_func_info_array(client_data->funcs, 0);
	}
	
	for (size_t i = 0; i < client_data->funcs->size; ++i) {
		FuncInfo *working_func_info = &client_data->funcs->data[i];
		working_func_info->access_footprint = init_var_access_array();

		build_var_access_footprint(working_func_info,
					   working_func_info->access_footprint);
	}

	for (size_t i = 0; i < client_data->funcs->size; ++i) {
		print_var_access_array(client_data->funcs->data[i].access_footprint, 0);
	}

free_tu_array:
	free_tu_array(tu_array, client_data->file_names->count);

free_priv:
	prong_free_priv(client_data);

exit:
	return ret;
}

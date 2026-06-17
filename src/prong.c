#include <stdio.h>
#include <stdlib.h>
#include <clang-c/Index.h>
#include <clang-c/CXCompilationDatabase.h>

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
	prong_error_t ret = ERR_OK;

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

	if (arg_verbose) {
		printf("%s[info]%s using clang arguments: ", CLR_VAR, CLR_RESET);
		aos_print_strings(client_data->extra_args);
		printf("\n");
	
		printf("%s[info]%s parsing files: ", CLR_VAR, CLR_RESET);
		aos_print_strings(client_data->file_names);
		printf("\n");
		
		printf("%s[info]%s searching function CXCursors: ", CLR_VAR, CLR_RESET);
		aos_print_strings(client_data->func_names);
		printf("\n");
	
		if (client_data->trace_var_names) {
			printf("%s[info]%s tracing variables: ", CLR_VAR, CLR_RESET);
			aos_print_strings(client_data->trace_var_names);
			printf("\n");
	
		}
	
	}

	if (arg_help) {
		print_usage(argv[0]);
		goto free_priv;
	}

	CXIndex index = clang_createIndex(0, 0);

	client_data->tu_array = alloc_tu_array(client_data->file_names->count);

	if (!client_data->tu_array) {
		ret = ERR_OUT_OF_MEMORY;
		goto free_priv;
	}

	CXCompilationDatabase_Error error = CXCompilationDatabase_NoError;
	CXCompilationDatabase db = NULL;
	if (client_data->compdb_dir) {
		printf("%s[info]%s compilation database directory: %s\n", CLR_VAR, CLR_RESET, client_data->compdb_dir);
		
		db = clang_CompilationDatabase_fromDirectory(
			client_data->compdb_dir,
			&error
		);
		
		if (error == CXCompilationDatabase_CanNotLoadDatabase) {
			print_error("Couldn't load compilation database at %s\n", client_data->compdb_dir);
			print_error("Error code CXCompilationDatabase_CanNotLoadDatabase\n");
			db = NULL;
		}
	}

	size_t tu_count = client_data->file_names->count;
	for (size_t i = 0; i < client_data->file_names->count; ++i) {
		/* If compilation DB exists, use it's arguments for compilation */
		if (db != NULL) {
			reset_aos(&client_data->clang_args);
			CXCompileCommands commands = clang_CompilationDatabase_getCompileCommands(
				db,
				client_data->file_names->strings[i]
			);

			unsigned int num_commands = clang_CompileCommands_getSize(commands);
			for (unsigned int j = 0; j < num_commands; ++j) {
				CXCompileCommand command = clang_CompileCommands_getCommand(commands, j);
				unsigned int num_args = clang_CompileCommand_getNumArgs(command);
				for (unsigned int k = 1; k < num_args-2; ++k) { /* start at 1 to skip 'clang' */
					CXString arg_str = clang_CompileCommand_getArg(command, k);
					if (strcmp(clang_getCString(arg_str), client_data->file_names->strings[i]) == 0 ||
					    strcmp(clang_getCString(arg_str), "clang") == 0 ||
					    strcmp(clang_getCString(arg_str), "-c") == 0)
						continue;
					aos_push_string(client_data->clang_args, clang_getCString(arg_str));
					clang_disposeString(arg_str);
				}
			}
			clang_CompileCommands_dispose(commands);
		}
		
		// Append custom clang arguments from cmdline
		aos_append_strings(client_data->clang_args, client_data->extra_args);

		if (arg_verbose) {
			printf("%s[info]%s using clang arguments for file %s:", CLR_VAR, CLR_RESET, client_data->file_names->strings[i]);
			aos_print_strings(client_data->clang_args);
			printf("\n");
		}

		printf("Parsing translation unit #%zu...\n", i);
		enum CXErrorCode parse_err = clang_parseTranslationUnit2(
			index,
			aos_string_at(client_data->file_names, i), 
			(const char * const*)client_data->clang_args->strings,
			client_data->clang_args->count,
			NULL, 0, CXTranslationUnit_None, 
			&client_data->tu_array[i]
		);

		if (parse_err != 0) {
			print_error("Unable to parse translation unit %zu. CXErrorCode = %d. Quitting\n", i, parse_err);
			ret = ERR_TU; // it didn't find the file or whatever
			tu_count = i;
			goto free_tu_array;
		}

		size_t num_diagnostics = clang_getNumDiagnostics(client_data->tu_array[i]);
		bool has_error = false;
		
		if (num_diagnostics == 0)
			continue;

		for (size_t j = 0; j < num_diagnostics; ++j) {
			CXDiagnostic diag = clang_getDiagnostic(client_data->tu_array[i], j);
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
					print_warn("Warning: \n\t%s\n", clang_getCString(formatted));
					clang_disposeString(formatted);
					break;
				default:
			}
			clang_disposeDiagnostic(diag);
		}

		if (has_error) {
			print_error("Errors detected parsing translation unit %zu\n", i);
			ret = ERR_SYNTAX;
			tu_count = i;
			goto free_tu_array;
		}
	}
	if (db)
		clang_CompilationDatabase_dispose(db);

	process_tu_array(client_data->tu_array, client_data);

	if (client_data->funcs->size != client_data->func_names->count) {
		print_error("Could not find some functions you were looking for.\n");
		ret = ERR_NOT_FOUND;
		goto free_tu_array;
	}

	for (size_t i = 0; i < client_data->funcs->size; ++i) {
		process_func_info(client_data->funcs->data+i, client_data);
	}
	
	reset_aos(&client_data->touched_func_usrs);
	
	//// Done with this stage, probably won't need this anymore
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

	/* Build array of variable accesses that contains ALL
	 * variable accesses recursively */
	for (size_t i = 0; i < client_data->funcs->size; ++i) {
		FuncInfo *working_func_info = &client_data->funcs->data[i];
		working_func_info->access_footprint = init_var_access_array();
		
		build_var_access_footprint(working_func_info,
					   working_func_info->access_footprint);
	}

	if (client_data->trace_var_names) {
		for (size_t f = 0; f < client_data->funcs->size; ++f) {
			FuncInfo *func_info = &client_data->funcs->data[f];
			find_exclusive_va_names(func_info, client_data);
		}
		goto free_tu_array;
	}
	// Messiest part :(
	/* Basically checks if two FuncInfos contain the same VarAccess by
	 * using the collection of all variable accesses from both functions 
	 * (access_footprint)*/
	for (size_t i = 0; i < client_data->funcs->size; ++i) {
		for (size_t j = 0; j < client_data->funcs->size; ++j) {
			if (i == j)
				continue;

			FuncInfo *func_info_i = &client_data->funcs->data[i];
			FuncInfo *func_info_j = &client_data->funcs->data[j];
			
			find_va_overlap(func_info_i, func_info_j, client_data);
		}
	}

free_tu_array:
	if (client_data->tu_array)
		free_tu_array(client_data->tu_array, tu_count);

free_priv:
	prong_free_priv(client_data);

exit:
	printf("%sDone.%s\n", CLR_VAR, CLR_RESET);
	return ret;
}

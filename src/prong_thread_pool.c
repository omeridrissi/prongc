#include "prong_thread_pool.h"

ThreadInfo init_thread_pool(size_t num_threads)
{
	ThreadInfo info_struct = {0};

	info_struct.threads = malloc(sizeof(pthread_t)*num_threads);
	info_struct.num_threads = num_threads;
	info_struct.num_active = 0;
	info_struct.next_file_idx = 0;
	pthread_mutex_init(&info_struct.mutex, NULL);

	return info_struct;
}

void destroy_thread_pool(ThreadInfo *thread_info) 
{
	free(thread_info->threads);

	thread_info->num_threads = 0;
	thread_info->num_active = 0;
	thread_info->next_file_idx = 0;
	pthread_mutex_destroy(&thread_info->mutex);
}

void *parse_file_thread(void *arg)
{
	struct prong_priv *prong_priv = (struct prong_priv*)arg;
	ThreadInfo *thread_info = &prong_priv->thread_info;

	prong_error_t ret = ERR_OK;

	while (1) {
		pthread_mutex_lock(&thread_info->mutex);
		
		size_t tu_idx = thread_info->next_file_idx;
		thread_info->next_file_idx++;

		if (tu_idx >= prong_priv->file_names->count) {
			thread_info->num_active--;
			pthread_mutex_unlock(&thread_info->mutex);
			break;
		}
		
		pthread_mutex_unlock(&thread_info->mutex);
	
		DynamicAOS *clang_args = init_aos();
		printf("Parsing file '%s'\n", prong_priv->file_names->strings[tu_idx]);
	
		if (prong_priv->db != NULL) {
			CXCompileCommands commands = clang_CompilationDatabase_getCompileCommands(
				prong_priv->db,
				prong_priv->file_names->strings[tu_idx]
			);

			unsigned int num_commands = clang_CompileCommands_getSize(commands);
			for (unsigned int j = 0; j < num_commands; ++j) {
				CXCompileCommand command = clang_CompileCommands_getCommand(commands, j);
				unsigned int num_args = clang_CompileCommand_getNumArgs(command);
				for (unsigned int k = 1; k < num_args; ++k) { /* start at 1 to skip 'clang' */
					CXString arg_str = clang_CompileCommand_getArg(command, k);
					if (strcmp(clang_getCString(arg_str), prong_priv->file_names->strings[tu_idx]) == 0 ||
					    strcmp(clang_getCString(arg_str), "clang") == 0 ||
					    strcmp(clang_getCString(arg_str), "-c") == 0 ||
					    strcmp(clang_getCString(arg_str), "--") == 0)
						continue;
					aos_push_string(clang_args, clang_getCString(arg_str));
					clang_disposeString(arg_str);
				}
			}
			clang_CompileCommands_dispose(commands);
		}
		// Append custom clang arguments from cmdline
		aos_append_strings(clang_args, prong_priv->extra_args);

		CXIndex index = clang_createIndex(0, 0);

		enum CXErrorCode parse_err = clang_parseTranslationUnit2(
			index,
			prong_priv->file_names->strings[tu_idx],
			(const char * const*)clang_args->strings,
			(int)clang_args->count,
			NULL, 0, CXTranslationUnit_None, 
			&prong_priv->tu_array[tu_idx]
		);
	
		if (parse_err != 0) {
			print_error("Unable to parse translation unit %s.\n CXErrorCode = %d. Quitting\n", 
					prong_priv->file_names->strings[tu_idx], parse_err);
			ret = parse_err;
			free_aos(clang_args);
			goto exit_thread;
		}


		free_aos(clang_args);
	}
	
exit_thread:
	print_debug("parsing thread: cleanup: exitting\n");

	pthread_exit(0);
}


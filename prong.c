#include <stdio.h>
#include <errno.h>
#include <clang-c/Index.h>
#include "core.h"

int main(int argc, char **argv) 
{
	struct prong_priv *client_data;
	int ret = 0;
	int files_pos, funcs_pos;
	int num_files, num_funcs;

	if (argc < 2) {
		print_usage();
		ret = -EINVAL;
		goto exit;
	}

	process_args(argc, argv, &files_pos, &funcs_pos,
		     &num_files, &num_funcs);

	if (files_pos == 0 || funcs_pos == 0) {
		print_usage();
		ret = -EINVAL;
		goto exit;
	}

	client_data = prong_init_priv(argv, funcs_pos, num_funcs);
	if (!client_data) {
		ret = -ENOMEM;
		goto exit;
	}

	CXIndex index = clang_createIndex(0, 0);

	CXTranslationUnit *tu_array = alloc_tu_array(num_files);
	if (!tu_array) {
		ret = -ENOMEM;
		goto free_priv;
	}

	for (int i = files_pos; i < (files_pos+num_files); ++i) {
		CXTranslationUnit unit = clang_parseTranslationUnit(
			index,
			argv[i], NULL, 0,
			NULL, 0,
			CXTranslationUnit_None
		);

		if (!unit) {
			printf("Unable to parse translation unit. Quitting.\n");
			ret = -EINVAL;
			goto free_tu_array;
		}

		tu_array[i-files_pos] = unit;
	}

	process_tu_array(tu_array, num_files, client_data);

free_tu_array:
	free(tu_array);

free_priv:
	prong_free_priv(client_data);

exit:
	return ret;
}

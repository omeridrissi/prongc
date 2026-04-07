#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <clang-c/Index.h>

struct prong_priv {
	char **func_names;
	int num_funcs;
	CXCursor *cursors;
	int num_cursors;
};

CXTranslationUnit	*alloc_tu_array(int length);
CXCursor		*alloc_cursor_array(int length);
void process_tu_array(CXTranslationUnit *tu_array, int length, void *client_data);
struct prong_priv *prong_init_priv(char **argv, int funcs_pos, int num_funcs);
void *prong_free_priv(struct prong_priv *prong_priv);

void process_args(int argc, char **argv, 
		  int *files_pos, int *funcs_pos,
		  int *num_files, int *num_funcs);
void print_usage(void);

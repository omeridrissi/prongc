#include <clang-c/Index.h>

#pragma once

typedef struct {
	size_t capacity;
	size_t size;
	char *data;
} DynamicAOS;

typedef enum {
	ERR_OK = 0,
	ERR_NOT_FOUND,
	ERR_INVALID_ARG,
	ERR_OUT_OF_MEMORY,
	ERR_IO
} error_t;

/* The prong_priv struct will hold the program
 * state and contain various types of data that
 * our program will collect over time */
struct prong_priv {
	char **func_names;
	char **file_names;
	int num_funcs;
	int num_files;
	CXCursor *cursors;
	int num_cursors;
	int num_cursors_filled;
	DynamicAOS *global_usrs;
	DynamicAOS *local_usrs;
	error_t err;
};


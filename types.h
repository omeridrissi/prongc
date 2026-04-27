#include <clang-c/Index.h>

#pragma once

/* Dynamic string array utility layer struct */
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

/* Forward declaration */
typedef struct FuncInfo FuncInfo;

typedef struct {
	size_t capacity;
	size_t size;
	FuncInfo *data;
} FuncInfoArr;

/* This struct will represent the functions we
 * recursively process over time */
struct FuncInfo {
	CXCursor	*cursor;	// The FunctionDecl itself
	char		*usr;	// USR of current function
	char		*name;	// Name of the element, for debug info
	DynamicAOS	*params;// USRs of parameters
	DynamicAOS	*locals;// USRs for local VarDecls
	
	/* For dependency tracking */
	FuncInfoArr	*callees;

	bool processed;		// Have we visited it's body yet?
};

/* The prong_priv struct will hold the program
 * state and contain various types of data that
 * our program will collect and use over time */
struct prong_priv {
	char		**func_names;	// The func names we get from cmdline
	char		**file_names;	// The file names we get from cmdline
	int		num_funcs;	// Number of function names
	int		num_files;	// Number of files
	DynamicAOS	*global_usrs;	// Bag of collected USRs of all global variables
	/* Function registry declaration */
	FuncInfoArr	*funcs;
	FuncInfo	*current_func; // Current traversal state
	/* Visitor function error return */
	error_t		err;		
};


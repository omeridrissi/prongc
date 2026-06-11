#include <clang-c/Index.h>

#pragma once

/* Dynamic string array utility layer struct */
typedef struct {
	size_t capacity;
	size_t size;
	size_t count;
	char *data;	// Packed string data
	char **strings; // Pointer to each string
} DynamicAOS;

typedef enum {
	ERR_OK = 0,
	ERR_NOT_FOUND,
	ERR_TU,
	ERR_SYNTAX,
	ERR_INVALID_ARG,
	ERR_OUT_OF_MEMORY,
	ERR_AOS_EMPTY,
	ERR_IO
} error_t;

/* Variable access types */
typedef enum {
	VarAccess_Read = 0,	// "x = var1"
	VarAccess_Write,	// "var1 = y"
	VarAccess_PtrRead,	// reading the value pointer points to
	VarAccess_PtrWrite,	// writing into the value pointer points to
	VarAccess_Escape,	// "func(var1)", variable passed as param
				// to function that we may or may not be able to 
				// recurse through,
				// and variable is a pointer or struct.
	VarAccess_Null,		// This is for when we don't need the struct
				// anymore so we null it out
} VarAccessType;

/* Struct that will represent a single
 * variable access */
typedef struct {
	char		*usr;
	char		*name;
	char		*esc_func_usr; // Only if passed to function
	size_t		esc_param_idx; // Paremeter idx when passed to func
	int		line;
	int		column;
	VarAccessType	type;
	bool		is_ptr_type;
} VarAccess;

typedef struct {
	size_t		size;
	size_t		capacity;
	VarAccess	*data;
} VarAccessArr;

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
	CXCursor	cursor;	// The FunctionDecl cursor itself
	char		*usr;	// USR of current function
	char		*name;	// Name of the element, for debug info
	DynamicAOS	*params;// USRs of parameters
	DynamicAOS	*locals;// USRs for local VarDecls
	
	VarAccessArr	*access_footprint; // All non-nullified VarAccesses
					   // including callee variable accesses in order
	
	/* For dependency tracking */
	FuncInfoArr	*callees;

	VarAccessArr	*var_accesses;

	bool in_system_header;	// Is function definition inside of
				// system header?
	bool has_definition;	// We couldn't find the function defnition
};

typedef struct {
	size_t		size;
	size_t		capacity;
	CXCursor	*data;
} CXCursorArr;

/* The prong_priv struct will hold the program
 * state and contain various types of data that
 * our program will collect and use over time */
struct prong_priv {
	CXTranslationUnit *tu_array;	// Translation unit array
	DynamicAOS	*func_names;	// The func names we get from cmdline
	DynamicAOS	*file_names;	// The file names we get from cmdline
	DynamicAOS	*clang_args;	// Arguments to be passed to libclang

	DynamicAOS	*global_usrs;	// Bag of collected USRs of all global variables
	
	/* While recursing through the children of a cursor,
	 * stack all the parent elements back to back here,
	 * then pop the tail when ending the recursion function */
	CXCursorArr	*ancestry_stack;

	CXCursor	last_visited;

	/* Function registry declaration */
	FuncInfoArr	*funcs;
	FuncInfo	*current_func; // Current traversal state
	DynamicAOS	*touched_func_usrs; // List of processed funcs

	/* Visitor function error return */
	error_t		err;		
};


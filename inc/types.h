
#pragma once

#include <stdbool.h>
#include <clang-c/Index.h>
#include <clang-c/CXCompilationDatabase.h>
#include <pthread.h>

struct lock_protection_field {
	unsigned int	lock_obj_hash; // based on lock object USR

	unsigned int	lock_func_hash;	// Numbers representing lock/unlock func names
	unsigned int	unlock_func_hash;

//	VarAccess	*lock_va; // Pointer to LockAcquire VA
};

typedef struct {
	struct lock_protection_field *lp_fields;
	size_t		size;
	size_t		capacity;
} ProtectionFieldArray;

typedef struct {
	const char *lock_func;
	const char *unlock_func;
} LockPrimitivePair;

typedef struct {
	LockPrimitivePair	*data;
	size_t			size;
} LockPairArray;

typedef struct {
	pthread_t	*threads;
	size_t		num_threads;
	size_t		next_file_idx; // Next file to parse
	pthread_mutex_t mutex;
	pthread_mutex_t err;		// For printing errors
	size_t		num_active;
} ThreadInfo;

/* Dynamic string array utility layer struct */
typedef struct {
	size_t capacity;
	size_t size;
	size_t count;
	size_t strings_cap; // strings[] capacity
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
} prong_error_t;

/* Variable access types */
typedef enum {
	/* Access types */
	VarAccess_Read = 0,	// "x = var1"
	VarAccess_Write,	// "var1 = y"
	VarAccess_PtrRead,	// reading the value pointer points to
	VarAccess_PtrWrite,	// writing into the value pointer points to
	VarAccess_Escape,	// "func(var1)", variable passed as param
				// to function that we may or may not be able to 
				// recurse through,
				// and variable is a pointer or struct.
	/* Placeholder types (not necessarily tied to any variable) */
	VarAccess_LockAcquire,	// passed to locking function like mutex_lock
	VarAccess_LockRelease,	// passed to unlock function like mutex_unlock
	VarAccess_Call,		// doesn't represent variable modification, instead
				// serves as filler to mark function calls that might
				// not accept any arguments
	VarAccess_IfStmt,	// if statement
	VarAccess_ElseIfStmt,	// else if statement
	VarAccess_ElseStmt,	// else statement
	VarAccess_EndIf,	// end of if statement block

	VarAccess_Null,		// This is for when we don't need the struct
				// anymore so we null it out
} VarAccessType;

/* Struct that will represent a single
 * variable access */
typedef struct {
	char		*usr;
	char		*name;
	char		*parent_func_name; // Reference to function name, not freeable
	char		*esc_func_spelling;
	char		*esc_func_usr; // Only if passed to function
	size_t		esc_param_idx; // Paremeter idx when passed to func
	struct lock_protection_field *lp_field; // Info about lock protection being held
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
	char		*name;	// Name of the cursor
	char		*spelling; // just name without param types
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
	CXCompilationDatabase db;	// compile_database.json
	DynamicAOS	*func_names;	// The func names we get from cmdline
	DynamicAOS	*file_names;	// The file names we get from cmdline
	DynamicAOS	*trace_var_names;// Function parameters or global variables 
					 // to trace exclusively
	char		*compdb_dir;	// Path to compilation database directory
	DynamicAOS	*trace_usrs;	// USRs of variables to trace exclusively
	DynamicAOS	*clang_args;	// Arguments to be passed to libclang
	DynamicAOS	*extra_args;	// These are appended to prong_priv->clang_args
	
	DynamicAOS	*lock_pairs;	// Additional lock/unlock funcs specified in cmd-line

	ThreadInfo	thread_info;	// Info about threads that will parse our files
	size_t		max_threads;	// Maximum number of concurrent threads

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
	prong_error_t		err;		
};


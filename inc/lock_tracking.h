#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

#include "types.h"
#include "log.h"

typedef enum {
	Shared_Protected = 0,
	Shared_Unprotected = 1,
	Shared_Uncertain = 2,
} SharedProtectionQuality;

#define NUM_LP_FIELDS 10

#define PRINT_LOCK_PAIR(lock_pair_ptr) print_lock_pair((lock_pair_ptr), #lock_pair_ptr)

void print_lock_pair(LockPrimitivePair *lock_pair, const char *lock_pair_name);

VarAccessType lock_primitive_type(const char *esc_func_spelling);

void init_lock_pair_array(LockPairArray *array, size_t size);
void free_lock_pair_array(LockPairArray *array);

LockPrimitivePair get_pair_by_lock_func(LockPairArray *lock_pair_array,
					const char *lock_func_name);
LockPrimitivePair get_pair_by_unlock_func(LockPairArray *lock_pair_array,
					  const char *unlock_func_name);

bool lock_pair_is_null(LockPrimitivePair *pair);

prong_error_t aos_to_lock_pair_array(DynamicAOS *array, LockPairArray *lock_pairs);

void init_lp_field(struct lock_protection_field *lp_field,
		   LockPrimitivePair primitive_pair,
		   VarAccess *lock_va,
		   size_t prot_range_start);
void destroy_lp_field(struct lock_protection_field *lp_field);

void init_protection_field_array(ProtectionFieldArray *lp_field_array);
void destroy_protection_field_array(ProtectionFieldArray *lp_field_array);

bool lp_field_is_null(struct lock_protection_field *lp_field);

void add_lock_protection_field(ProtectionFieldArray *lp_field_array,
				LockPrimitivePair primitive_pair,
				VarAccess *lock_va, 
				size_t prot_range_start);
void remove_lock_protection_field(ProtectionFieldArray *lp_field_array,
				  const char *lock_obj_usr);
void set_protection_range_end(ProtectionFieldArray *lp_field_array,
			      const char *lock_obj_usr, size_t prot_range_end);

void add_protection_range_fields_active(ProtectionFieldArray *lp_field_array,
				 size_t prot_range_start, size_t branch_depth);
void add_protection_range_fields_inactive(ProtectionFieldArray *lp_field_array,
				 size_t prot_range_start, size_t branch_depth);

bool lp_field_array_contains(ProtectionFieldArray *lp_field_haystack,
			     struct lock_protection_field *lp_field_needle);

bool equal_lp_fields(struct lock_protection_field *lp_field_a,
		     struct lock_protection_field *lp_field_b);

SharedProtectionQuality matching_protection_field_arrays(ProtectionFieldArray *lp_field_array_a,
				      ProtectionFieldArray *lp_field_array_b,
				      size_t va_idx_i, size_t va_idx_j);

void print_lp_field_array(ProtectionFieldArray *lp_field_array);


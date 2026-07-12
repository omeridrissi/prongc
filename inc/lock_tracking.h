#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "types.h"
#include "log.h"

#define NUM_LP_FIELDS 4

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
		   VarAccess *lock_va);
void destroy_lp_field(struct lock_protection_field *lp_field);

void init_protection_field_array(ProtectionFieldArray *lp_field_array);
void destroy_protection_field_array(ProtectionFieldArray *lp_field_array);

bool lp_field_is_null(struct lock_protection_field *lp_field);

void add_lock_protection_field(ProtectionFieldArray *lp_field_array,
				LockPrimitivePair primitive_pair,
				VarAccess *lock_va);
void remove_lock_protection_field(ProtectionFieldArray *lp_field_array,
				  const char *lock_obj_usr);

bool equal_lp_fields(struct lock_protection_field *lp_field_a,
		     struct lock_protection_field *lp_field_b);


#pragma once

#include "lock_tracking.h"

LockPrimitivePair default_lock_pairs[] = {
	{"mutex_lock", "mutex_unlock"},
	{"mutex_lock_interruptible", "mutex_unlock"},
	{"mutex_lock_killable", "mutex_unlock"},
	{"spin_lock", "spin_unlock"},
	{"spin_lock_irq", "spin_unlock_irq"},
	{"spin_lock_irqsave", "spin_unlock_irqrestore"},
	{"spin_lock_bh", "spin_unlock_bh"},
	{"raw_spin_lock", "raw_spin_unlock"},
	{"raw_spin_lock_irq", "raw_spin_unlock_irq"},
	{"raw_spin_lock_irqsave", "raw_spin_lock_irqrestore"},
};

size_t num_default_pairs = sizeof(default_lock_pairs)/sizeof(default_lock_pairs[0]);

VarAccessType lock_primitive_type(const char *esc_func_spelling) 
{
	for (size_t i = 0; i < num_default_pairs; ++i) {
		LockPrimitivePair *prim_pair = &default_lock_pairs[i];
		if (strcmp(esc_func_spelling, prim_pair->lock_func) == 0)
			return VarAccess_LockAcquire;
		else if (strcmp(esc_func_spelling, prim_pair->unlock_func) == 0)
			return VarAccess_LockAcquire;
		else
			continue;
	}

	return VarAccess_Null;
}

void init_lock_pair_array(LockPairArray *array, size_t size)
{
	if (size == 0) {
		array->data = NULL;
		array->size = 0;
		return;
	}
	array->data = malloc(sizeof(LockPrimitivePair)*size);
	array->size = size;
}

void free_lock_pair_array(LockPairArray *array) 
{
	free(array->data);
	array->data = NULL;
	array->size = 0;
}

/* loops through each string and splits it by replacing ':' with '\0',
 * sets lock and unlock function names in all of the lock pairs.
 * Returns an error if unlock_func is not set or ':' is not found */
prong_error_t aos_to_lock_pair_array(DynamicAOS *array, LockPairArray *lock_pairs)
{
	for (size_t i = 0; i < array->count; ++i) {
		char *current_str = array->strings[i];
		LockPrimitivePair *pair = &lock_pairs->data[i];

		pair->lock_func = current_str;
		while (*current_str != '\0') {
			if (*current_str == ':') {
				*current_str++ = '\0';
				pair->unlock_func = current_str;
				break;
			}
			current_str++;
		}

		if (pair->unlock_func == NULL)
			return ERR_INVALID_ARG;
	}	

	return ERR_OK;
}

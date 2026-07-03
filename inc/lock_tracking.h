#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"

VarAccessType lock_primitive_type(const char *esc_func_spelling);

void init_lock_pair_array(LockPairArray *array, size_t size);
void free_lock_pair_array(LockPairArray *array);

prong_error_t aos_to_lock_pair_array(DynamicAOS *array, LockPairArray *lock_pairs);


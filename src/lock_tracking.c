#include <stddef.h>
#include "lock_tracking.h"
#include "var_access.h"

LockPrimitivePair default_lock_pairs[] = {
	/* Userspace locking */
	{"pthread_mutex_lock", "pthread_mutex_unlock"},
	{"pthread_mutex_trylock", "pthread_mutex_unlock"},
	{"pthread_spin_lock", "pthread_spin_unlock"},
	{"pthread_spin_trylock", "pthread_spin_unlock"},
	/* Linux Kernel locking primitives*/
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

void print_lock_pair(LockPrimitivePair *lock_pair, const char *lock_pair_name)
{
	printf("%s = (\"%s\", \"%s\")\n", lock_pair_name,
					lock_pair->lock_func,
					lock_pair->unlock_func);
}

VarAccessType lock_primitive_type(const char *esc_func_spelling) 
{
	for (size_t i = 0; i < num_default_pairs; ++i) {
		LockPrimitivePair *prim_pair = &default_lock_pairs[i];
		if (strcmp(esc_func_spelling, prim_pair->lock_func) == 0)
			return VarAccess_LockAcquire;
		else if (strcmp(esc_func_spelling, prim_pair->unlock_func) == 0)
			return VarAccess_LockRelease;
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

LockPrimitivePair get_pair_by_lock_func(LockPairArray *lock_pair_array,
					const char *lock_func_name)
{
	if (lock_func_name != NULL) {
		for (size_t i = 0; i < lock_pair_array->size; ++i) {
			if (strcmp(lock_pair_array->data[i].lock_func, lock_func_name) == 0) {
				return lock_pair_array->data[i];
			}
		}
	}
	return (LockPrimitivePair){0};
}

LockPrimitivePair get_pair_by_unlock_func(LockPairArray *lock_pair_array,
					  const char *unlock_func_name)
{
	for (size_t i = 0; i < lock_pair_array->size; ++i) {
		if (strcmp(lock_pair_array->data[i].unlock_func, unlock_func_name) == 0) {
			return lock_pair_array->data[i];
		}
	}
	return (LockPrimitivePair){0};
}

bool lock_pair_is_null(LockPrimitivePair *pair) 
{
	if (pair->lock_func == NULL ||
	    pair->unlock_func == NULL)
		return true;
	else
		return false;
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
			} else if (!isalnum(*current_str) && *current_str != '_') {
				print_error("'%c' is not alphanumeric, exitting...\n", *current_str);
				return ERR_SYNTAX;
			}
			current_str++;
		}

		if (pair->unlock_func == NULL) {
			print_error("pair->unlock_func is null, exitting...\n");
			return ERR_SYNTAX;
		}
	}	

	return ERR_OK;
}

#define FNV_OFFSET_BASIS	0x811c9dc5
#define FNV_PRIME		0x01000193	
static unsigned int lock_tracking_gen_hash(const char *str_in)
{
	unsigned int hash = FNV_OFFSET_BASIS;

	while (*str_in != '\0') {
		hash *= FNV_PRIME;
		hash ^= (unsigned char)*str_in;

		str_in++;
	}

	return hash;
}

void init_lp_field(struct lock_protection_field *lp_field,
		   LockPrimitivePair primitive_pair,
		   VarAccess *lock_va, 
		   size_t prot_range_start)
{
	const char *lock_func = primitive_pair.lock_func;
	const char *unlock_func = primitive_pair.unlock_func;

	lp_field->lock_va = lock_va;

	/* Makes a scattered linked list order in our main array */
	lp_field->next_prot_range = NULL; 
	lp_field->branch_depth = 0;
	
	lp_field->protection_range_start = prot_range_start;
	lp_field->protection_range_end = SIZE_MAX;

	lp_field->lock_func_hash = lock_tracking_gen_hash(lock_func);
	lp_field->unlock_func_hash = lock_tracking_gen_hash(unlock_func);
	lp_field->lock_obj_hash = lock_tracking_gen_hash(lock_va->usr);

	lp_field->is_ll_start = true; // this function is only used for start ranges so ts always true
}

void destroy_lp_field(struct lock_protection_field *lp_field) 
{
	memset(lp_field, '\0', sizeof(struct lock_protection_field));
}

void init_protection_field_array(ProtectionFieldArray *lp_field_array)
{
	lp_field_array->lp_fields = malloc(sizeof(struct lock_protection_field)*NUM_LP_FIELDS);
	// zero everything out
	memset(lp_field_array->lp_fields, '\0', 
	       sizeof(struct lock_protection_field)*NUM_LP_FIELDS);
	lp_field_array->size = 0;
	lp_field_array->capacity = NUM_LP_FIELDS;
}

void destroy_protection_field_array(ProtectionFieldArray *lp_field_array)
{
	free(lp_field_array->lp_fields);
	lp_field_array->size = 0;
	lp_field_array->capacity = 0;
}

bool lp_field_is_null(struct lock_protection_field *lp_field) 
{
	if (!lp_field->lock_obj_hash)
		return true;
	else
		return false;
}

/* Updates lp_field->next_prot_range pointers to match new allocation */
static void update_lp_field_array_pointers(ProtectionFieldArray *lp_field_array,
					   uintptr_t old_base_ptr)
{
	uintptr_t new_base_ptr = (uintptr_t)lp_field_array->lp_fields;
	ptrdiff_t realloc_offset = (ptrdiff_t)(new_base_ptr - old_base_ptr);
	for (size_t i = 0; i < lp_field_array->size; ++i) {
		struct lock_protection_field *lp_field = &lp_field_array->lp_fields[i];

		if (lp_field->next_prot_range != NULL) {
			lp_field->next_prot_range = 
				(struct lock_protection_field*)((char*)lp_field->next_prot_range + realloc_offset);
		}
	}
}

/* Adds a lock protection field to the lp_field stack,
 * grows dynamically if more space is needed, any added
 * memory is nulled out */
void add_lock_protection_field(ProtectionFieldArray *lp_field_array,
				LockPrimitivePair primitive_pair,
				VarAccess *lock_va, 
				size_t prot_range_start) 
{
	size_t lp_array_size = lp_field_array->size;
	if (lp_array_size >= lp_field_array->capacity) {
		size_t old_cap = lp_field_array->capacity;
		lp_field_array->capacity *= 2;
		uintptr_t old_base_ptr = (uintptr_t)lp_field_array->lp_fields;

		lp_field_array->lp_fields = reallocarray(lp_field_array->lp_fields,
							 lp_field_array->capacity,
							 sizeof(struct lock_protection_field));
		memset(lp_field_array->lp_fields+lp_array_size, '\0',
			lp_field_array->capacity - old_cap);
		update_lp_field_array_pointers(lp_field_array, old_base_ptr);
	}

	struct lock_protection_field *lp_field = &lp_field_array->lp_fields[lp_array_size];
	
	init_lp_field(lp_field, primitive_pair, lock_va, prot_range_start);

	lp_field_array->size++;
}

struct lock_protection_field *add_lock_protection_copy(ProtectionFieldArray *lp_field_array,
			      struct lock_protection_field *lp_field_in)
{
	size_t lp_array_size = lp_field_array->size;
	if (lp_array_size >= lp_field_array->capacity-1) {
		size_t old_cap = lp_field_array->capacity;
		lp_field_array->capacity *= 2;
		uintptr_t old_base_ptr = (uintptr_t)lp_field_array->lp_fields;

		lp_field_array->lp_fields = reallocarray(lp_field_array->lp_fields,
							 lp_field_array->capacity,
							 sizeof(struct lock_protection_field));	
		memset(lp_field_array->lp_fields+lp_array_size, '\0',
			lp_field_array->capacity - old_cap);
		update_lp_field_array_pointers(lp_field_array, old_base_ptr);
	}

	struct lock_protection_field *lp_field = &lp_field_array->lp_fields[lp_array_size];
	*lp_field = *lp_field_in;
	lp_field->is_ll_start = false;
	lp_field->next_prot_range = NULL;

	lp_field_array->size++;

	return lp_field;
}

struct lock_protection_field *get_lp_field_linked_list_end(struct lock_protection_field *lp_field)
{
	struct lock_protection_field *working_lp_field = lp_field;
	while (working_lp_field->next_prot_range != NULL) {
		working_lp_field = working_lp_field->next_prot_range;
	}
	
	return working_lp_field;
}

void set_protection_range_end(ProtectionFieldArray *lp_field_array,
			      const char *lock_obj_usr, size_t prot_range_end)
{
	unsigned int lock_obj_hash = lock_tracking_gen_hash(lock_obj_usr);
	for (size_t i = 0; i < lp_field_array->size; ++i) {
		struct lock_protection_field *lp_field = &lp_field_array->lp_fields[i];
		if (lp_field->protection_range_end == SIZE_MAX && 
		    lp_field->lock_obj_hash == lock_obj_hash) {
			struct lock_protection_field *lp_tail = get_lp_field_linked_list_end(lp_field);
			lp_tail->protection_range_end = prot_range_end;
		}
	}
}

/* Duplicates lock protection fields currently active inside of this branch
 * and extends their range to disconnected sections in a linked list arrangement */
void add_protection_range_fields_active(ProtectionFieldArray *lp_field_array,
					size_t prot_range_start, size_t branch_depth)
{
	size_t lp_field_arr_size = lp_field_array->size;
	for (size_t i = 0; i < lp_field_arr_size; ++i) {
		struct lock_protection_field *lp_field = &lp_field_array->lp_fields[i];
		if (lp_field->protection_range_end == SIZE_MAX) { // Finds any protection that's still active
			struct lock_protection_field *new_lp_field = add_lock_protection_copy(lp_field_array, lp_field);
			
			lp_field->next_prot_range = new_lp_field;
			lp_field->protection_range_end = prot_range_start; // ends where the new segment begins
		
			lp_field->branch_depth = branch_depth;

			new_lp_field->protection_range_start = prot_range_start;
			new_lp_field->protection_range_end = SIZE_MAX;
		}
	}
}

void add_protection_range_fields_inactive(ProtectionFieldArray *lp_field_array,
					    size_t prot_range_start, size_t branch_depth)
{
	size_t lp_field_arr_size = lp_field_array->size;
	for (size_t i = 0; i < lp_field_arr_size; ++i) {
		struct lock_protection_field *lp_field = &lp_field_array->lp_fields[i];
		// Filters for protections who'se end range had been set in other branches
		if (lp_field->next_prot_range == NULL &&
		    lp_field->protection_range_end != SIZE_MAX &&
		    lp_field->branch_depth == branch_depth) {
			struct lock_protection_field *new_lp_field = add_lock_protection_copy(lp_field_array, lp_field);
		
			lp_field->next_prot_range = new_lp_field;
			
			/* add new uncapped protection range */
			new_lp_field->protection_range_start = prot_range_start;
			new_lp_field->protection_range_end = SIZE_MAX;
		}
	}
}

bool lp_field_array_contains(ProtectionFieldArray *lp_field_haystack,
			     struct lock_protection_field *lp_field_needle)
{
	for (size_t i = 0; i < lp_field_haystack->size; ++i) {
		if (equal_lp_fields(&lp_field_haystack->lp_fields[i], lp_field_needle)) 
			return true;
	}

	return false;
}

bool equal_lp_fields(struct lock_protection_field *lp_field_a,
		     struct lock_protection_field *lp_field_b)
{
	return (lp_field_a->lock_obj_hash == lp_field_b->lock_obj_hash);
}

static bool lp_cond_prot(struct lock_protection_field *lp_field,
		         size_t va_idx) 
{
	return (lp_field->next_prot_range != NULL &&
		lp_field->protection_range_end == SIZE_MAX &&
		lp_field->branch_depth != 0);
}

// I'm sorry you have to read this

static void get_lp_fields_in_range(ProtectionFieldArray *lp_field_array,
				    struct lock_protection_field **lp_fields,
				    size_t *num_protections,
				    size_t va_idx)
{
	for (size_t i = 0; i < lp_field_array->size; ++i) {
		struct lock_protection_field *lp_field = &lp_field_array->lp_fields[i];
		int ll_depth = 0;
		if (lp_field->is_ll_start) { // make sure we only pulling base prot ranges
next:			
			print_debug("ll_depth = %d\n", ll_depth);
			bool conditional_prot = lp_cond_prot(lp_field, va_idx);

			if ((va_idx > lp_field->protection_range_start &&
			     va_idx < lp_field->protection_range_end) ||
			    conditional_prot) {
				lp_fields[*num_protections] = lp_field; // in range so active
				*num_protections += 1;
			}
		
			if (lp_field->next_prot_range != NULL) {
				lp_field = lp_field->next_prot_range;
				ll_depth++;
				print_debug("nexting\n");
				goto next;
			}
			
			continue;
		}
	}
}

bool lp_fields_contain(struct lock_protection_field **lp_fields, size_t num_fields,
		       struct lock_protection_field *lp_field)
{
	for (size_t i = 0; i < num_fields; ++i) {
		if (equal_lp_fields(lp_fields[i], lp_field))
			return true;
	}

	return false;
}

SharedProtectionQuality matching_protection_field_arrays(ProtectionFieldArray *lp_field_array_a,
							 ProtectionFieldArray *lp_field_array_b,
							 size_t va_idx_i, size_t va_idx_j)
{
	/* copy over active protection instances for our indices */
	struct lock_protection_field *active_protections_a[lp_field_array_a->size],
				     *active_protections_b[lp_field_array_b->size];

	memset(&active_protections_a, '\0', sizeof(void*)*lp_field_array_a->size);
	memset(&active_protections_b, '\0', sizeof(void*)*lp_field_array_b->size);
	size_t num_protections_a = 0;
	size_t num_protections_b = 0;

	get_lp_fields_in_range(lp_field_array_a, active_protections_a, &num_protections_a, va_idx_i);
	get_lp_fields_in_range(lp_field_array_b, active_protections_b, &num_protections_b, va_idx_j);

	if (num_protections_a != num_protections_b)
		return Shared_Unprotected;

	for (size_t i = 0; i < num_protections_a; ++i) {
		struct lock_protection_field *lp_field_a = active_protections_a[i];
		
		bool contains = false;
		for (size_t j = 0; j < num_protections_b; ++j) {
			struct lock_protection_field *lp_field_b = active_protections_b[j];
			//if (lp_cond_prot(lp_field_a, va_idx_i) || lp_cond_prot(lp_field_b, va_idx_j))
			//	return Shared_Uncertain;

			if (lp_field_b->branch_depth != 0 || lp_field_b->branch_depth != 0)
				return Shared_Uncertain;
			if (equal_lp_fields(lp_field_a, lp_field_b)) {
				contains = true;
				break;
			}
		}

		if (!contains) {
			return Shared_Unprotected;
		}
	}

	return Shared_Protected;
}

void print_lp_field_array(ProtectionFieldArray *lp_field_array)
{
	for (size_t i = 0; i < lp_field_array->size; ++i) {
		struct lock_protection_field *lp_field = &lp_field_array->lp_fields[i];
		printf("Printing lp field array: \n");
		printf("    | lock func hash: %u\n", lp_field->lock_func_hash);
		printf("    | unlock func hash: %u\n", lp_field->unlock_func_hash);
		printf("    | lock obj hash: %u\n", lp_field->lock_obj_hash);
		printf("    | start idx: %zu\n", lp_field->protection_range_start);
		printf("    | end idx: %zu\n", lp_field->protection_range_end);
		print_var_access(lp_field->lock_va, 2);
	}
}


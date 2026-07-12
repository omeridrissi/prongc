#include "lock_tracking.h"

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

LockPrimitivePair get_pair_by_lock_func(LockPairArray *lock_pair_array,
				     const char *lock_func_name)
{
	for (size_t i = 0; i < lock_pair_array->size; ++i) {
		if (strcmp(lock_pair_array->data[i].lock_func, lock_func_name) == 0) {
			return lock_pair_array->data[i];
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

#define HASH_CONST 15
static unsigned int lock_tracking_gen_hash(const char *str_in)
{
	unsigned int result = 0;
	bool toggle_shifting = false;

	while (*str_in != '\0') {
		if (toggle_shifting) {
			result += (unsigned int)str_in;
			result = (result >> 1)*HASH_CONST;
			toggle_shifting = false;
		} else {
			result += (unsigned int)str_in;
			result = (result << 2)*HASH_CONST;
			toggle_shifting = true;
		}
		str_in++;
	}

	print_debug("generated hash for \"%s\" -> \"%u\"\n", str_in, result);

	return result;
}

void init_lp_field(struct lock_protection_field *lp_field,
		   LockPrimitivePair primitive_pair,
		   VarAccess *lock_va)
{
	const char *lock_func = primitive_pair.lock_func;
	const char *unlock_func = primitive_pair.unlock_func;

	lp_field->lock_va = lock_va;
	lp_field->lock_func_hash = lock_tracking_gen_hash(lock_func);
	lp_field->unlock_func_hash = lock_tracking_gen_hash(unlock_func);
	lp_field->lock_obj_hash = lock_tracking_gen_hash(lock_va->usr);
}

void destroy_lp_field(struct lock_protection_field *lp_field) 
{
	memset(lp_field, '\0', sizeof(struct lock_protection_field));
}

void init_protection_field_array(ProtectionFieldArray *lp_field_array)
{
	lp_field_array->lp_fields = malloc(sizeof(ProtectionFieldArray)*NUM_LP_FIELDS);
	// zero everything out
	memset(lp_field_array->lp_fields, '\0', 
	       sizeof(ProtectionFieldArray)*NUM_LP_FIELDS);
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

/* Adds a lock protection field to the lp_field stack,
 * grows dynamically if more space is needed, any added
 * memory is nulled out */
void add_lock_protection_field(ProtectionFieldArray *lp_field_array,
				LockPrimitivePair primitive_pair,
				VarAccess *lock_va) 
{
	size_t lp_array_size = lp_field_array->size;
	size_t old_cap = lp_field_array->capacity;
	if (lp_array_size >= lp_field_array->capacity) {
		lp_field_array->capacity *= 2;
		lp_field_array->lp_fields = reallocarray(lp_field_array->lp_fields,
							 lp_field_array->capacity,
							 sizeof(struct lock_protection_field));
		memset(lp_field_array->lp_fields+lp_array_size, '\0',
			lp_field_array->capacity-lp_array_size);
		init_lp_field(&lp_field_array->lp_fields[lp_array_size], 
			      primitive_pair, lock_va);
	}

	if (lp_array_size < old_cap) {
		for (size_t i = 0; i < lp_array_size; ++i) {
			struct lock_protection_field *lp_field = &lp_field_array->lp_fields[i];
			if (!lp_field_is_null(lp_field)) {
				init_lp_field(&lp_field_array->lp_fields[i],
						primitive_pair, lock_va);
				break;
			}
		}
	}

	lp_field_array->size++;
}

void remove_lock_protection_field(ProtectionFieldArray *lp_field_array,
				  const char *lock_obj_usr)
{
	unsigned int lock_obj_hash = lock_tracking_gen_hash(lock_obj_usr);
	for (size_t i = 0; i < lp_field_array->size; ++i) {
		struct lock_protection_field *lp_field = &lp_field_array->lp_fields[i];
		if (lp_field->lock_obj_hash == lock_obj_hash) {
			destroy_lp_field(lp_field);
			lp_field_array->size--;
			return;
		}
	}
}

bool lp_field_array_contains(ProtectionFieldArray *lp_field_array,
			     struct lock_protection_field *lp_field) 
{

}

bool equal_lp_fields(struct lock_protection_field *lp_field_a,
		     struct lock_protection_field *lp_field_b) 
{
	return (lp_field_a->lock_obj_hash == lp_field_b->lock_obj_hash);
}

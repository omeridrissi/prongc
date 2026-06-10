/* This file will contain all the function definitions
 * for our "dynamic array of strings" utility layer.
 * The strings will be stored in one single contiguous
 * allocated dynamic memory block on the heap. This is 
 * better for performance because of cache locality
 * (fewer cache misses).
 * Our program will only push onto this array and free 
 * the whole thing it finishes, so this approach is most
 * likely valid for our use case here. */

#include "dyn_aos.h"

/* Initiate DynamicAOS struct fields, data is initially
 * predetermined with AOS_INITIAL_SIZE macro */
DynamicAOS *init_aos(void)
{
	DynamicAOS *array_struct;

	array_struct = malloc(sizeof(*array_struct));

	array_struct->data = (char*)malloc(sizeof(char)*AOS_INITIAL_CAP);
	array_struct->strings = (char**)malloc(sizeof(char*)*AOS_MAX_STR_COUNT);
	array_struct->capacity = AOS_INITIAL_CAP;
	array_struct->size = 0;
	array_struct->count = 0;

	return array_struct;
}

/* Frees all the struct's allocated fields */
void free_aos(DynamicAOS *array) 
{
	free(array->data);
	free(array->strings);
	array->capacity = 0;

	/* Makes it easier to check for bugs later on */
	array->data = NULL;
}

void reset_aos(DynamicAOS **array)
{
	free_aos(*array);
	*array = init_aos();
}

/* Update all string values to new ones */
static void aos_update_old_strings(DynamicAOS *array) {
	size_t offset = 0;
	for (size_t i = 0; i < array->count; ++i) {
		char *current = array->data + offset;
		array->strings[i] = current;
		offset += strlen(current)+1;
	}
}

/* Push string on top of the array, automatically 
 * realloc if not enough size */
error_t aos_push_string(DynamicAOS *array, const char *str)
{
	size_t str_size = strlen(str)+1;

	/* Bounds check. Double array size if evaluates to true */
bounds_check:
	if ((array->size + str_size) > array->capacity) {
		size_t new_cap = array->capacity ? array->capacity*2 : AOS_INITIAL_CAP;
		array->data = reallocarray(array->data, new_cap, sizeof(char));

		if (!array->data)
			return ERR_OUT_OF_MEMORY;

		array->capacity = new_cap;
		aos_update_old_strings(array); // array->strings[] now points to old freed buffer.
					       // udpates all array->strings[] values to fit new one.

		goto bounds_check;
	}

	memcpy(array->data + array->size, str, str_size);
	array->strings[array->count] = array->data + array->size;
	array->size += str_size;
	array->count++;

	return ERR_OK;
}

/* Pop */
error_t aos_pop_string(DynamicAOS *array) {
	if (array->count == 0)
		return ERR_AOS_EMPTY;
	
	const char *last_str = aos_string_at(array, array->count-1);
	size_t last_str_size = strlen(last_str)+1;

	array->size -= last_str_size;
	array->count--;

	return ERR_OK;
}

/* Returns number of tightly packed strings
 * in the memory block */
ssize_t aos_string_count(DynamicAOS *array)
{
	return array->count;
}

/* Returns the string's memory location if
 * present, returns NULL otherwise */
char *aos_find_string(DynamicAOS *array, const char *needle)
{
	for (size_t i = 0; i < array->count; ++i) {
		char *current = array->strings[i];
		if (strcmp(current, needle) == 0) {
			return current;
		}
	}

	return NULL;
}

/* Returns true if string is present in array,
 * returns false otherwise */
bool aos_contains_string(DynamicAOS *array, const char *needle)
{
	for (size_t i = 0; i < array->count; ++i) {
		const char *current = array->strings[i];
		if (strcmp(current, needle) == 0) {
			return true;
		}
	}

	return false;
}

/* Searches for certain string in the array,
 * returns it's index if found, returns -1 if 
 * string is not present */
ssize_t aos_find_string_idx(DynamicAOS *array, const char *needle)
{
	for (size_t i = 0; i < array->count; ++i) {
		const char *current = array->strings[i];
		if (strcmp(current, needle) == 0) {
			return i;
		}
	}

	return -1;
}

/* Returns the beginning of the string at 
 * certain idx in order, returns NULL if 
 * index is out of range. */
char *aos_string_at(DynamicAOS *array, size_t idx) 
{
	if (idx > array->count-1)
		return NULL;
	
	return array->strings[idx];
}

/* Prints all the strings in the array */
void aos_print_strings(DynamicAOS *array) 
{
	if (array->count == 0) {
		printf("[ empty ]");
		return;
	}
	printf("[ ");
	for (size_t i = 0; i < array->count; ++i) {
		const char *current = array->strings[i];
		printf("\"");
		printf("%s", current);
		printf("\", ");	
	}
	printf("]");
}

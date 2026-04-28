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
	array_struct->capacity = AOS_INITIAL_CAP;
	array_struct->size = 0;

	return array_struct;
}

/* Frees all the struct's allocated fields */
void free_aos(DynamicAOS *array) 
{
	free(array->data);
	array->capacity = 0;
	array->capacity = 0;

	/* Makes it easier to check for bugs later on */
	array->data = NULL;
}

/* Push string on top of the array, automatically 
 * realloc if not enough size */
error_t aos_push_string(DynamicAOS *array, const char *str)
{
	size_t str_size = strlen(str)+1;

	/* Bounds check. Double array size if evaluates to true */
bounds_check:
	if ((array->size + str_size) > array->capacity) {
		array->data = reallocarray(array->data, 
					   array->capacity, 
					   sizeof(char)*2);
		if (!array->data)
			return ERR_OUT_OF_MEMORY;
		array->capacity = array->capacity*2;
		goto bounds_check;
	}

	memcpy(array->data + array->size, str, str_size);
	array->size += str_size;
	array->count++;

	return 0;
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
	size_t offset = 0;
	while (offset <= array->size) {
		char *current = array->data + offset;
		if (strcmp(current, needle) == 0) {
			return current;
		}
		offset += strlen(current) + 1;
	}

	return NULL;
}

/* Returns true if string is present in array,
 * returns false otherwise */
bool aos_contains_string(DynamicAOS *array, const char *needle)
{
	size_t offset = 0;
	while (offset <= array->size) {
		const char *current = array->data + offset;
		if (strcmp(current, needle) == 0) {
			return true;
		}
		offset += strlen(current) + 1;
	}

	return false;
}

/* Searches for certain string in the array,
 * returns it's index if found, returns -1 if 
 * string is not present */
ssize_t aos_find_string_idx(DynamicAOS *array, const char *needle)
{
	int idx = 0;
	size_t offset = 0;
	while (offset <= array->size) {
		const char *current = array->data + offset;
		++idx;
		if (strcmp(current, needle) == 0) {
			return idx;
		}
		offset += strlen(current)+1;
	}

	return -1;
}

/* Returns the beginning of the string at 
 * certain idx in order, returns NULL if 
 * index is out of range. */
char *aos_string_at(DynamicAOS *array, size_t idx) 
{
	size_t counter = 0;
	size_t offset = 0;
	while (offset <= array->size) {
		const char *current = array->data + offset;
		if (counter++ == idx)
			return current;
		offset += strlen(current)+1;
	}

	return NULL;
}

/* Prints all the strings in the array */
void aos_print_strings(DynamicAOS *array) 
{
	if (array->count == 0) {
		printf("[ empty ]");
		return;
	}
	printf("[ ");
	size_t n = array->count;
	size_t offset = 0;
	for (size_t i = 0; i < n; ++i) {
		const char *current = array->data + offset;
		printf("\"");
		printf("%s", current);
		printf("\", ");
		
		offset += strlen(current)+1;
	}
	printf("]");
}

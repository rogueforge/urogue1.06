#include <stdio.h>
#include <stdlib.h>

#include "dict.h"
#include "memory.h"

static char sccsid[] = "%W%\t%G%";

/*	Debugging memory allocation code that tries to trap common memory problems
	like overwriting storage and stepping on memory pointer chains. If code
	doesn't use malloc, free, and realloc a lot, these routines can be left in
	as added protection against undetected storage bugs.
*/

/* 	FENCE_SIZE should be a multiple of sizeof(size_t) to prevent alignment problems.
	The code assumes that malloc and realloc return pointers aligned at least on size_t
	sized boundaries and that a pointer needs alignment no more strict than that of an
	object needed to hold a size_t.
*/

#define FENCE_SIZE (sizeof(size_t) * 300)

static int memdebug_level = 0;
static DICTIONARY *allocations = NULL;

/* set the debug level */
void mem_debug(const int level)
{
	memdebug_level = level;

	/* all except 0, 1, and unknown fall through */
	switch(memdebug_level) {
	case 2:
		printf("+++ Memory tracking possible, ");
	case 1:
		printf("+++ Memory debugging enabled, ");
		break;
	case 0:
		printf("+++ Memory debugging disabled, ");
		break;
	default:
		printf("!!! Unknown memory debug level set, enabling level 1, ");
		memdebug_level = 1;
		break;
	}
	printf("fence size = %d\n", FENCE_SIZE);
}

/* set memory tracking on or off */
/* turning it off deletes all tracking data */
void mem_tracking(int flag)
{
	/* do nothing if debuglevel is too low */
	if (memdebug_level < 2)
		return;

	/* turn on tracking */
	if (flag > 0) {
		if (allocations != NULL) {
			dict_destroy(allocations);
			allocations = NULL;
		}
		allocations = dict_create(8, 100, 4, 20);
		if (allocations == NULL) {
			printf("!!! Unable to allocate tracking table!\n");
			abort();
		}
	}
	/* turn off tracking */
	else if (allocations != NULL) {
		dict_destroy(allocations);
		allocations = NULL;
	}
}

/* go through all pointers and see if they are OK, aborting if not */
/* always returns 1 if not aborting so that it can be included in  */
/* if statement boolean expressions */
int mem_check(void)
{
	STRING_ENTRY *se;

	/* scan of a NULL dictionary always succeeds */
	if (allocations == NULL)
		return TRUE;

	if (!dict_scan_begin(allocations)) {
		printf("!!! Dictionary scan initialization failed!\n");
		abort();
	}

	printf("+++ --- Starting pointer scan\n");

	/* mem_validate aborts if there is a problem */
	while((se = dict_scan_next(allocations)) != NULL)
		mem_validate(se->any_ptr);

	printf("+++ --- Done pointer scan\n");

	/* always return a good value if execution arrives here */
	return 1;
}

/* allocate some memory and initialize header and trailer */
void *mem_malloc(const size_t bytes)
{
	char *mem_temp;
	size_t real_size = bytes + (FENCE_SIZE << 1);

	/* allocate including guard bytes to detect some ways of overwriting of memory areas */
	mem_temp = (void *)malloc(real_size);
	if (memdebug_level > 0) {
		printf("+++ Requested size of %ld bytes\n", bytes);
		printf("+++ Actual malloc of %ld bytes located at %p\n", real_size, mem_temp);
	}

	/* if allocation succeeded, set management data */
	if (mem_temp != NULL) {
		long i;
		char *end;

		/* do beginning marker bytes */
		for (i = 0; i < FENCE_SIZE - sizeof(size_t); i++)
			*mem_temp++ = 145;

		/* save size in header too */
		if (memdebug_level > 0)
			printf("*** Requested memory size stored at %p\n", mem_temp);
		*(size_t *)mem_temp = bytes;

		/* finally, point to storage we are going to hand out */
		mem_temp += sizeof(size_t);

		/* now, point to trailer bytes and do them */
		end = mem_temp + bytes;
		for (i = 0; i < FENCE_SIZE; i++)
			*end++ = 145;
	}

	/* track pointer if needed */
	if (memdebug_level > 1 && allocations != NULL) {
		char key[16];
		long temp;

		sprintf(key, "%p", mem_temp);
		if (dict_insert(allocations, key, 1, 0, mem_temp, &temp) == NULL) {
			printf("!!! Insert of pointer tracking info failed\n");
			abort();
		}
	}

	/* allow caller to do error handling */
	if (memdebug_level > 0) {
		printf("--- Returning pointer of %p\n", mem_temp);
		fflush(stdout);
	}
	return (void *)mem_temp;
}

/* release some memory, making sure that it was properly allocated */
void mem_free(const void *ptr)
{
	char *mem_temp = (char *)ptr - FENCE_SIZE;

	if (memdebug_level > 0)
		printf("+++ Free of memory located at %p\n", ptr);
	if (ptr == NULL) {
		if (memdebug_level > 0) {
			printf("!!! Freeing NULL pointer\n");
			fflush(stdout);
		}
		abort();
	}

	mem_validate(ptr);	/* doesn't return on error */

	/* if doing memory tracking */
	if (memdebug_level > 1 && allocations != NULL) {
		char key[16];
		STRING_ENTRY *se;
		long temp;

		sprintf(key, "%p", ptr);

		if ((se = dict_search(allocations, key, &temp)) == NULL) {
			printf("!!! Deleting pointer not found in tracking info\n");
			abort();
		}

		if (se->count == 0) {
			printf("!!! Freeing a pointer that has already been freed!\n");
			abort();
		}

		/* remember deleted stuff by zeroing the allocation count */
		se->count = 0;
	}

	if (memdebug_level > 0)
		fflush(stdout);
	free((void *)mem_temp);
}

/* reallocate some memory, making sure that it was properly allocated */
void *mem_realloc(const void *ptr, const size_t new_size)
{
	char *mem_temp = (char *)ptr;
	size_t real_size = new_size + (FENCE_SIZE << 1);
	long i;

	if (memdebug_level > 0) {
		printf("+++ Requested size of %ld bytes\n", new_size);
		printf("+++ Actual malloc of %ld bytes located at %p\n", real_size, mem_temp);
	}
	if (ptr == NULL) {
		if (memdebug_level > 0) {
			printf("!!! Reallocating NULL pointer\n");
			fflush(stdout);
		}
		abort();
	}

	mem_validate(ptr);	/* doesn't return on error */

	/* if doing memory tracking */
	if (memdebug_level > 1 && allocations != NULL) {
		char key[16];
		STRING_ENTRY *se;
		long temp;

		sprintf(key, "%p", ptr);

		if ((se = dict_search(allocations, key, &temp)) == NULL) {
			printf("!!! Deleting pointer not found in tracking info\n");
			abort();
		}

		if (se->count == 0) {
			printf("!!! Freeing a pointer that has already been freed!\n");
			abort();
		}

		/* remember deleted stuff by zeroing the allocation count */
		se->count = 0;
	}

	/* header marker bytes will be copied by the realloc */
	mem_temp = (char *)ptr - FENCE_SIZE;
	mem_temp = realloc((void *)mem_temp, real_size);

	if (mem_temp != NULL) {
		char *end;

		/* save size in header too */
		mem_temp += FENCE_SIZE - sizeof(size_t);
		if (memdebug_level > 0)
			printf("*** Requested memory size stored at %p\n", mem_temp);
		*(size_t *)mem_temp = new_size;

		/* finally, point to storage we are going to hand out */
		mem_temp += sizeof(size_t);

		/* now, point to trailer bytes and do them */
		end = mem_temp + new_size;
		for (i = 0; i < FENCE_SIZE; i++)
			*end++ = 145;
	}

	if (memdebug_level > 1 && allocations != NULL) {
		char key[16];
		long temp;

		sprintf(key, "%p", mem_temp);
		if (dict_insert(allocations, key, 1, 0, mem_temp, &temp) == NULL) {
			printf("!!! Insert of pointer tracking info failed\n");
			abort();
		}
	}

	if (memdebug_level > 0) {
		printf("--- Returning pointer of %p\n", mem_temp);
		fflush(stdout);
	}
	return (void *)mem_temp;
}

/* check a pointer to be sure all check bytes are OK. abort if not */
/* always returns 1 if not aborting so that it can be included in  */
/* if statement boolean expressions */
int mem_validate(void *ptr)
{
	char *mem_temp = (char *)ptr;
	long i;

	/* NULL pointers are always valid */
	if (ptr == NULL)
		return 1;

	if (memdebug_level > 0)
		printf("+++ Checking %p as pointer\n", ptr);

	/* check the header bytes */
	mem_temp -= FENCE_SIZE;
	if (memdebug_level > 0)
		printf("+++ Real pointer at %p\n", mem_temp);

	for (i = 0; i < FENCE_SIZE - sizeof(size_t); i++)
		if (*mem_temp++ != 145) {
			if (memdebug_level > 0) {
				printf("!!! The user pointer at %p has been overwritten\n", ptr);
				printf("!!! Header offset %ld has been changed\n", i - 1);
				fflush(stdout);
			}
			abort();
		}

	/* check size */
	i = *(size_t *)mem_temp;
	if (memdebug_level > 0)
		printf("*** Stored memory size of %ld bytes in header\n", i);

	/* now point to where trailer should be */
	mem_temp = (char *)ptr + i;
	for (i = 0; i < FENCE_SIZE; i++)
		if (*mem_temp++ != 145) {
			if (memdebug_level > 0) {
				printf("!!! The user pointer at %p has been overwritten\n", ptr);
				printf("!!! Trailer offset %ld has been changed\n", i - 1);
				fflush(stdout);
			}
			abort();
		}
	if (memdebug_level > 0)
		fflush(stdout);
	return 1;
}

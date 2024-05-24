//------------------------------------------------------------------------------
//
// memtrace
//
// trace calls to the dynamic memory manager
//
#define _GNU_SOURCE

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <memlog.h>
#include <memlist.h>

//
// function pointers to stdlib's memory management functions
//
static void *(*mallocp)(size_t size) = NULL;
static void (*freep)(void *ptr) = NULL;
static void *(*callocp)(size_t nmemb, size_t size);
static void *(*reallocp)(void *ptr, size_t size);

//
// statistics & other global variables
//
static unsigned long n_malloc  = 0;
static unsigned long n_calloc  = 0;
static unsigned long n_realloc = 0;
static unsigned long n_allocb  = 0;
static unsigned long n_freeb   = 0;
static item *list = NULL;

//
// init - this function is called once when the shared library is loaded
//
__attribute__((constructor))
void init(void)
{
	char *error;

	LOG_START();

	// initialize a new list to keep track of all memory (de-)allocations
	// (not needed for part 1)
	list = new_list();

	// ...
}

//
// fini - this function is called once when the shared library is unloaded
//
__attribute__((destructor))
void fini(void)
{
	long n_alloc_total = n_malloc + n_calloc + n_realloc;
	long avg_allocb = n_allocb / n_alloc_total;
	item* node = list;
	bool found = false;

	LOG_STATISTICS(n_allocb, avg_allocb, n_freeb);

	while ((node = node->next) != NULL) {
		if (node->cnt > 0) {
			if (!found) {
				LOG_NONFREED_START();
				found = true;
			}
			LOG_BLOCK(node->ptr, node->size, node->cnt);
		}
	}

	LOG_STOP();

	// free list (not needed for part 1)
	free_list(list);
}

// ...

void* malloc(size_t size) {
	void* ptr;

	// Get address of libc malloc
	if (!mallocp) {
		mallocp = dlsym(RTLD_NEXT, "malloc");
	}

	ptr = mallocp(size);
	LOG_MALLOC(size, ptr);
	
	n_malloc++;
	n_allocb += size;

	alloc(list, ptr, size);

	return ptr;
}

void free(void* ptr) {
	item* node;

	LOG_FREE(ptr);

	// validity check
	node = find(list, ptr);
	if (node == NULL) {
		LOG_ILL_FREE();
		return;
	}
	if (node->cnt == 0) {
		LOG_DOUBLE_FREE();
		return;
	}

	if (!freep) {
		freep = dlsym(RTLD_NEXT, "free");
	}

	freep(ptr);

	node = dealloc(list, ptr);
	n_freeb += node->size;
}

void* calloc(size_t count, size_t size) {
	void* ptr;

	if (!callocp) {
		callocp = dlsym(RTLD_NEXT, "calloc");
	}

	ptr = callocp(count, size);
	LOG_CALLOC(count, size, ptr);

	n_calloc++;
	n_allocb += size;

	alloc(list, ptr, count*size);

	return ptr;
}

void* realloc(void* p, size_t size) {
	void* ptr;
	item* node;

	if (!reallocp) {
		reallocp = dlsym(RTLD_NEXT, "realloc");
	}

	ptr = reallocp(p, size);
	LOG_REALLOC(p, size, ptr);

	n_realloc++;
	n_allocb += size;

	node = dealloc(list, p);
	n_freeb += node->size;
	
	alloc(list, ptr, size);

	return ptr;
}

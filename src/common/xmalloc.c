/* 
 * $Id$ 
 *
 * Copyright (C) 2000 Regents of the University of California
 * See ./DISCLAIMER
 */

#if     HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>	/* for INT_MAX */
#include <assert.h>

#include "xmalloc.h"

#if	HAVE_UNSAFE_MALLOC
#include <pthread.h>
static pthread_mutex_t malloc_lock = PTHREAD_MUTEX_INITIALIZER;
#define MALLOC_LOCK()	pthread_mutex_lock(&malloc_lock)
#define MALLOC_UNLOCK()	pthread_mutex_unlock(&malloc_lock)
#else
#define MALLOC_LOCK()	
#define MALLOC_UNLOCK()	
#endif

/*
 * "Safe" version of malloc().
 *   size (IN)	number of bytes to malloc
 *   RETURN	pointer to allocate heap space
 */
void *
Malloc(size_t size)
{	
	void *new;
	int *p;
       
	assert(size > 0 && size <= INT_MAX);
	MALLOC_LOCK();
	p = (int *)malloc(size + 2*sizeof(int));
	MALLOC_UNLOCK();
	if (!p) {
		fprintf(stderr, "Malloc(%d) failed\n", size);
		exit(1);
	}
	p[0] = XMALLOC_MAGIC;			/* add "secret" magic cookie */
	p[1] = size;				/* store size in buffer */

	new = &p[2];
	memset(new, 0, size);
	return new;
}

/* 
 * "Safe" version of realloc().  Args are different: pass in a pointer to
 * the object to be realloced instead of the object itself.
 *   item (IN/OUT)	double-pointer to allocated space
 *   newsize (IN)	requested size
 */
void 
Realloc(void **item, size_t newsize)
{
	int *p = (int *)*item - 2;

	assert(*item != NULL);
       	assert(newsize > 0 && newsize <= INT_MAX);
	assert(p[0] == XMALLOC_MAGIC);		/* magic cookie still there? */

	MALLOC_LOCK();
	p = (int *)realloc(p, newsize + 2*sizeof(int));
	MALLOC_UNLOCK();
	if (!p) {
		fprintf(stderr, "Realloc(%d) failed\n", newsize);
		exit(1);
	}
	assert(p[0] == XMALLOC_MAGIC);
	p[1] = newsize;
	*item = &p[2];
}

/*
 * Duplicate a string.
 *   str (IN)		string to duplicate
 *   RETURN		copy of string
 */
char *
Strdup(char *str)
{
	char *result = Malloc(strlen(str) + 1);

	return strcpy(result, str);
}

/*
 * Return the size of a buffer.
 *   item (IN)		pointer to allocated space
 */
int 
Size(void *item)
{
	int *p = (int *)item - 2;

	assert(item != NULL);	
	assert(p[0] == XMALLOC_MAGIC);
	return p[1];
}

/* 
 * Free which takes a pointer to object to free, which it turns into a null
 * object.
 *   item (IN/OUT)	double-pointer to allocated space
 */
void 
Free(void **item)
{
	int *p = (int *)*item - 2;

	if (*item != NULL) {
		assert(p[0] == XMALLOC_MAGIC);	/* magic cookie still there? */
		MALLOC_LOCK();
		free(p);
		MALLOC_UNLOCK();
		*item = NULL;
	}
}

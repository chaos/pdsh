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

#include <assert.h>

#include "xmalloc.h"

/*
 * "Safe" version of malloc().
 *   size (IN)	number of bytes to malloc
 *   RETURN	pointer to allocate heap space
 */
void *xmalloc(size_t size)
{	
	void *new;
	int *p = (int *)malloc(size + sizeof(int));

	if (!p) {
		perror("malloc failed");
		exit(1);
	}
	p[0] = XMALLOC_MAGIC;			/* add "secret" magic cookie */

	new = &p[1];
	memset(new, 0, size);
	return new;
}

/* 
 * "Safe" version of realloc().  Args are different: pass in a pointer to
 * the object to be realloced instead of the object itself.
 *   item (IN/OUT)	double-pointer to allocated space or NULL
 *   newsize (IN)	requested size
 */
void xrealloc(void **item, size_t newsize)
{
	int *p = (int *)*item - 1;

	assert(*item != NULL && newsize != 0);
	assert(p[0] == XMALLOC_MAGIC);		/* magic cookie still there? */

	p = (int *)realloc(p, newsize + 1);
	if (!p) {
		perror("realloc failed");
		exit(1);
	}
	assert(p[0] == XMALLOC_MAGIC);
	*item = &p[1];
}

/* 
 * Free which takes a pointer to object to free, which it turns into a null
 * object.
 *   item (IN/OUT)	double-pointer to allocated space
 */
void xfree(void **item)
{
	int *p = (int *)*item - 1;

	if (*item != NULL) {
		assert(p[0] == XMALLOC_MAGIC);	/* magic cookie still there? */
		free(p);
		*item = NULL;
	}
}

/* 
 * $Id$ 
 *
 * Copyright (C) 2000 Regents of the University of California
 * See ./DISCLAIMER
 */

#include <conf.h>

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
	char *cp = (char *)malloc(size + 1);

	if (!cp) {
		perror("malloc failed");
		exit(1);
	}
	cp[0] = XMALLOC_MAGIC;			/* add "secret" magic cookie */

	new = &cp[1];
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
	char *cp = (char *)*item - 1;

	assert(*item != NULL && newsize != 0);
	assert(cp[0] == XMALLOC_MAGIC);		/* magic cookie still there? */

	cp = (char *)realloc(cp, newsize + 1);
	if (!cp) {
		perror("realloc failed");
		exit(1);
	}
	assert(cp[0] == XMALLOC_MAGIC);
	*item = &cp[1];
}

/* 
 * Free which takes a pointer to object to free, which it turns into a null
 * object.
 *   item (IN/OUT)	double-pointer to allocated space
 */
void xfree(void **item)
{
	char *cp = (char *)*item - 1;

	if (*item != NULL) {
		assert(cp[0] == XMALLOC_MAGIC);	/* magic cookie still there? */
		free(cp);
		*item = NULL;
	}
}

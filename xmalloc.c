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

/*
 * "Safe" version of malloc().
 *   size (IN)	number of bytes to malloc
 *   RETURN	pointer to allocate heap space
 */
void *xmalloc(size_t size)
{	
	char *new;

	new = (char *)malloc(size);
	if (!new) {
		perror("malloc failed");
		exit(1);
	}
	bzero(new, size);
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
	assert(*item != NULL && newsize != 0);
	*item = realloc(*item, newsize);
	if (!*item) {
		perror("realloc failed");
		exit(1);
	}
}

/* 
 * Free which takes a pointer to object to free, which it turns into a null
 * object.
 *   item (IN/OUT)	double-pointer to allocated space
 */
void xfree(void **item)
{
	if (*item != NULL) {
		free(*item);
		*item = NULL;
	}
}

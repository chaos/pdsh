/*
 * $Id$
 * $Source$
 */

#if     HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <stdlib.h>

#include "list.h"
#include "err.h"
#include "hostlist.h"
#include "hostlist_wrap.h"


/*
 * Helper for range_split() - allocate a new pdsh list_t and copy each hostname
 * element of a hostlist_t to it.
 */
static list_t
_copy_hostlist_to_list(hostlist_t hl)
{
	list_t l = list_new(); /* exits on malloc failure */
	hostlist_iterator_t itr; 
	char *host;
	
	if (!(itr = hostlist_iterator_create(hl)))
		errx("%p: hostlist_iterator_create failed\n");
	while ((host = hostlist_next(itr)))
		list_push(l, host);
	hostlist_iterator_destroy(itr);

	return l;
}

/* 
 * Wrapper for Mark's hostlist.c stuff to make it return a pdsh list_t.
 */
list_t 
range_split(char *list_sep, char *range_sep, char *str)
{
	hostlist_t hl; 
	list_t l;

	assert(strcmp(list_sep, ",") == 0);
	assert(strcmp(range_sep, "-")== 0);

	if (!(hl = hostlist_create(str)))
		errx("%p: hostlist_create failed\n");
	l = _copy_hostlist_to_list(hl);
	hostlist_destroy(hl);
	return l;
}

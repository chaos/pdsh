/*****************************************************************************\
 *
 *  $Id$
 *  $Source$
 *
 *  Copyright (C) 1998-2002 The Regents of the University of California.
 *  Produced at Lawrence Livermore National Laboratory (cf, DISCLAIMER).
 *  Written by Jim Garlick (garlick@llnl.gov>
 *  UCRL-CODE-980038
 *  
 *  This file is part of PDSH, a parallel remote shell program.
 *  For details, see <http://www.llnl.gov/linux/powerman/>.
 *  
 *  PDSH is free software; you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *  
 *  PDSH is distributed in the hope that it will be useful, but WITHOUT 
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License 
 *  for more details.
 *  
 *  You should have received a copy of the GNU General Public License along
 *  with PDSH; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.
\*****************************************************************************/

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

/* 
 * $Id$ 
 *
 * Copyright (C) 2000 Regents of the University of California
 * See ./DISCLAIMER
 */

#ifndef _XMALLOC_INCLUDED
#define _XMALLOC_INCLUDED

#include <sys/types.h>

void *xmalloc(size_t);
void xrealloc(void **, size_t);
void xfree(void **);

#endif /* _XMALLOC_INCLUDED */

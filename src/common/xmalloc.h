/* 
 * $Id$ 
 *
 * Copyright (C) 2000 Regents of the University of California
 * See ./DISCLAIMER
 */

#ifndef _XMALLOC_INCLUDED
#define _XMALLOC_INCLUDED

#include <sys/types.h>

void *Malloc(size_t);
void Realloc(void **, size_t);
void Free(void **);
int Size(void *);
char *Strdup(char *);

#define XMALLOC_MAGIC 0x42

#endif /* _XMALLOC_INCLUDED */

/* 
 * $Id$ 
 *
 * Copyright (C) 2000 Regents of the University of California
 * See ./DISCLAIMER
 */

#ifndef _XSTRING_INCLUDED
#define _XSTRING_INCLUDED

#include <stdio.h>

void xstrcln(char **, char *);
int xfgets(char **, int *, FILE *);

char *xstrduplicate(char *str, int *size);
void xstrcat(char **str1, int *size, char *str2);
void xstrcatchar(char **str1, int *size, char c);
void xstrerrorcat(char **str1, int *size);
char *xbasename(char *path);

#endif /* _XSTRING_INCLUDED */

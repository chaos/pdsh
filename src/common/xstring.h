/* 
 * $Id$ 
 *
 * Copyright (C) 2000-2002 Regents of the University of California
 * See ./DISCLAIMER
 */

#ifndef _XSTRING_INCLUDED
#define _XSTRING_INCLUDED

#include <stdio.h>

void xstrcln(char **, char *);
int xfgets(char **, FILE *);

char *xstrduplicate(char *str);
void xstrcat(char **str1, char *str2);
void xstrcatchar(char **str1, char c);
void xstrerrorcat(char **str1);
char *xbasename(char *path);

#endif /* _XSTRING_INCLUDED */

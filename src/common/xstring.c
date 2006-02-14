/*****************************************************************************\
 *  $Id$
 *****************************************************************************
 *  Copyright (C) 2001-2006 The Regents of the University of California.
 *  Produced at Lawrence Livermore National Laboratory (cf, DISCLAIMER).
 *  Written by Jim Garlick <garlick@llnl.gov>.
 *  UCRL-CODE-2003-005.
 *  
 *  This file is part of Pdsh, a parallel remote shell program.
 *  For details, see <http://www.llnl.gov/linux/pdsh/>.
 *  
 *  Pdsh is free software; you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *  
 *  Pdsh is distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *  details.
 *  
 *  You should have received a copy of the GNU General Public License along
 *  with Pdsh; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.
\*****************************************************************************/

/*
 * Heap-oriented string functions.
 */

#if     HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#if 	HAVE_STRERROR_R && !HAVE_DECL_STRERROR_R
char *strerror_r(int, char *, int);
#endif
#include <errno.h>
#if	HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <assert.h>

#include "xmalloc.h"
#include "xstring.h"

#define SPACES "\n\t "

#define XFGETS_CHUNKSIZE 32

/*
 * Zap leading and trailing occurrences of characters in 'verboten'.
 *   str (IN/OUT)	string
 *   verboten (IN)	list of characters to be zapped (if NULL, zap spaces)
 */
void xstrcln(char *str, char *verboten)
{
    char *p;
    char *base = str;

    if (verboten == NULL)
        verboten = SPACES;

    /* move pointer past initial 'verboten' characters */
    while (str != NULL && *str != '\0' && strchr(verboten, *str) != NULL)
        str++;

    /* overwrite trailing 'verboten' characters with nulls */
    if (str != NULL && strlen(str) > 0) {
        p = str + strlen(str) - 1;
        while (p > str && *p != '\0' && strchr(verboten, *p) != NULL)
            *p-- = '\0';
    }

    /* move string */
    assert(str >= base);
    memmove(base, str, strlen(str));
    while (str-- > base)
        base[strlen(base) - 1] = '\0';
}

/*
 * Ensure that a string has enough space to add 'needed' characters.
 * If the string is uninitialized, it should be NULL.
 */
static void _makespace(char **str, int needed)
{
    int used;

    if (*str == NULL)
        *str = Malloc(needed + 1);
    else {
        used = strlen(*str) + 1;
        while (used + needed > Size(*str)) {
            int newsize = Size(*str) + XFGETS_CHUNKSIZE;

            Realloc((void **) str, newsize);
            assert(Size(*str) == newsize);
        }
    }
}

/* 
 * Concatenate str2 onto str1, expanding str1 as needed.
 *   str1 (IN/OUT)	target string (pointer to in case of expansion)
 *   str2 (IN)		source string
 */
void xstrcat(char **str1, char *str2)
{
    _makespace(str1, strlen(str2));
    strcat(*str1, str2);
}

/* 
 * Copy str2 to str1, expanding str1 as needed.
 *   str1 (IN/OUT)	target string (pointer to in case of expansion)
 *   str2 (IN)		source string
 */
void xstrcpy(char **str1, char *str2)
{
    _makespace(str1, strlen(str2));
    strcpy(*str1, str2);
}

static void _strcatchar(char *str, char c)
{
    int len = strlen(str);

    str[len++] = c;
    str[len] = '\0';
}

/* 
 * Add a character to str, expanding str1 as needed.
 *   str1 (IN/OUT)	target string (pointer to in case of expansion)
 *   size (IN/OUT)	size of str1 (pointer to in case of expansion)
 *   c (IN)		character to add
 */
void xstrcatchar(char **str, char c)
{
    _makespace(str, 1);
    _strcatchar(*str, c);
}

void xstrerrorcat(char **buf)
{
#if HAVE_STRERROR_R
#  if HAVE_WORKING_STRERROR_R || STRERROR_R_CHAR_P
    char errbuf[64];
    char *err = strerror_r(errno, errbuf, 64);
#  else
    char err[64];
    strerror_r(errno, err, 64);
#  endif
#elif HAVE_STRERROR
    char *err = strerror(errno);
#else
    extern char *sys_errlist[];
    char *err = sys_errlist[errno];
#endif
    xstrcat(buf, err);
}


/* 
 * Replacement for libc basename
 *   path (IN)		path possibly containing '/' characters
 *   RETURN		last component of path
 */
char *xbasename(char *path)
{
    char *p;

    p = strrchr(path, '/');
    return (p ? (p + 1) : path);
}


/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

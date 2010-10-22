/*****************************************************************************\
 *  $Id$
 *****************************************************************************
 *  Copyright (C) 2006 The Regents of the University of California.
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

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "xmalloc.h"
#include "split.h"

/* 
 * Helper function for list_split(). Extract tokens from str.  
 * Return a pointer to the next token; at the same time, advance 
 * *str to point to the next separator.  
 *   sep (IN)   string containing list of separator characters
 *   str (IN)   double-pointer to string containing tokens and separators
 *   RETURN next token
 */
static char *_next_tok(char *sep, char **str)
{
    char *tok;
    int level = 0;

    /* push str past any leading separators */
    while (**str != '\0' && strchr(sep, **str) != NULL)
        (*str)++;

    if (**str == '\0')
        return NULL;

    /* assign token pointer */
    tok = *str;

    /* push str past token and leave pointing to first separator,
       ignoring separators between any '[]' */
    while (**str != '\0' && (level != 0 || strchr(sep, **str) == NULL)) {
        if (**str == '[')
            level++;
        else if (**str == ']')
            level--;
        (*str)++;
    }

    /* nullify consecutive separators and push str beyond them */
    while (**str != '\0' && strchr(sep, **str) != NULL)
        *(*str)++ = '\0';

    return tok;
}

static void free_f (char *str)
{
    Free ((void **) &str);
}

/*
 * Given a list of separators and a string, generate a list
 *   sep (IN)   string containing separater characters
 *   str (IN)   string containing tokens and separators
 *   RETURN     new list containing all tokens
 */
List list_split(char *sep, char *str)
{
    List new = list_create((ListDelF) free_f);
    char *tok;

    if (sep == NULL)
        sep = " \t";

    while ((tok = _next_tok(sep, &str)) != NULL) {
        if (strlen(tok) > 0)
            list_append(new, Strdup(tok));
    }

    return new;
}

List list_split_append (List l, char *sep, char *str)
{
    char *tok;

    if (l == NULL)
        return (list_split (sep, str));

    if (sep == NULL)
        sep = " \t";

    while ((tok = _next_tok(sep, &str)) != NULL) {
        if (strlen(tok) > 0)
            list_append(l, Strdup(tok));
    }

    return l;
}

int list_join (char *result, size_t len, const char *sep, List l)
{
    char *str = NULL;
    int n = 0;
    int truncated = 0;
    ListIterator i;

    memset (result, 0, len);
        
    if (list_count(l) == 0)
        return (0);
        
    i = list_iterator_create(l);
    while ((str = list_next(i))) {
        int count;
            
        if (!truncated)  {
            count = snprintf(result + n, len - n, "%s%s", str, sep); 

            if ((count >= (len - n)) || (count < 0)) 
                truncated = 1;
            else
                n += count;
        }
        else
            n += strlen (str) + strlen (sep);
    }
    list_iterator_destroy(i);

    if (truncated)
        result [len - 1] = '\0';
    else {
        /* 
         * Delete final separator
         */
        result[strlen(result) - strlen(sep)] = '\0';
    }

    return (n);
}

/* vi: ts=4 sw=4 expandtab
 */


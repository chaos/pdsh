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
 *  For details, see <http://www.llnl.gov/linux/pdsh/>.
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

#ifndef _LIST_INCLUDED
#define _LIST_INCLUDED

/* incomplete definition -- finished off in list.c for encapsulation */
typedef struct list_implementation *list_t;

list_t list_new();
void list_expand(list_t);
void list_free(list_t *);
void list_push(list_t, char *);
char *list_pop(list_t);
char *list_shift(list_t);
list_t list_split(char *, char *);
list_t list_split_range(char *, char *, char *);
void list_dump(list_t);
char *list_join(char *, list_t);
void list_pushl(list_t, list_t);
int list_test(list_t, char *);
void list_subtract(list_t, list_t);
void list_merge(list_t, list_t);
int list_length(list_t);
char *list_nth(list_t, int);
void list_del(list_t, int);

#endif                          /* _LIST_INCLUDED */

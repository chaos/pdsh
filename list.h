/* 
 * $Id$
 *
 * Copyright (C) 2000-2002 Regents of the University of California
 * See ./DISCLAIMER
 */

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
void list_merge(list_t, list_t);
int list_length(list_t);
char *list_nth(list_t, int);
void list_del(list_t, int);

#endif /* _LIST_INCLUDED */

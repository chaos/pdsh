/* 
 * $Id$ 
 * 
 * Copyright (C) 2000-2002 Regents of the University of California
 * See ./DISCLAIMER
 */

#if     HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <stdlib.h>

#include "list.h"
#include "xmalloc.h"
#include "xstring.h"
#include "err.h"

/*
 * number of entries to allocate to a list initially and at subsequent
 * expansions
 */
#define LIST_CHUNK      16

#define SPACES          "\n\t "

/*
 * max size of hostname range in list_split_range()
 * (8k hosts enough?)
 */
#define MAX_RANGE	8192

/* 
 * this completes the list_t type in list.h and prevents users of this
 * module from violating our abstraction.
 */
struct list_implementation {
#ifndef NDEBUG
#define LIST_MAGIC 	0xd00fd00f
        int magic;
#endif
        int size;
        int nitems;
        char **data;
};

static char *next_tok(char *, char **);

/*
 * Create a new list with LIST_CHUNK elements allocated.
 *   RETURN	newly allocated list.
 */
list_t 
list_new(void)
{
	list_t new = (list_t)Malloc(sizeof(struct list_implementation));
#ifndef NDEBUG
	new->magic = LIST_MAGIC;
#endif
	new->data = (char **)Malloc(LIST_CHUNK * sizeof(char *));
	new->size = LIST_CHUNK;
	new->nitems = 0;

	return new;
}

/*
 * Expand a list to accomodate LIST_CHUNK more elements.
 *   l (in)	list which will be expanded
 */
void 
list_expand(list_t l)
{
	assert(l->magic == LIST_MAGIC);
	l->size += LIST_CHUNK;
	Realloc((void **)&(l->data), l->size * sizeof(char *));
}

/*
 * Free a list, including all the strings stored in elements, and the list
 * structure itself.  The casts to void are to shuddup the aix xlc_r compiler.
 *   l (IN)	pointer to list (list will be set to NULL)
 */
void 
list_free(list_t *l)
{
	int i;

	assert((*l)->magic == LIST_MAGIC);
	for (i = 0; i < (*l)->nitems; i++)
		Free((void **)&((*l)->data[i]));
	Free((void **)&((*l)->data));
	Free((void **)l);
}
	
/*
 * Push a word onto list.
 *   l (IN)	list 
 *   word (IN)	word to be added to list
 */
void 
list_push(list_t l, char *word)
{
	assert(l->magic == LIST_MAGIC);
	if (l->size == l->nitems)
		list_expand(l);
	assert(l->size > l->nitems);
	l->data[l->nitems++] = Strdup(word);
}

/*
 * Pop a word off of list (caller is responsible for freeing word)
 *   l (IN)	list
 *   RETURN	last entry of list
 */
char *
list_pop(list_t l)
{
	char *word = NULL;

	assert(l->magic == LIST_MAGIC);
	if (l->nitems > 0)	
		word = l->data[--(l->nitems)];

	return word;
}

/*
 * Shift a word off list (caller is responsible for freeing word)
 *   l (IN)	list
 *   RETURN	first entry of list
 */
char *
list_shift(list_t l)
{
	char *word = NULL;
	int i;

	assert(l->magic == LIST_MAGIC);
	if (l->nitems) {
		word = l->data[0];
		for (i = 0; i < l->nitems; i++)
			l->data[i] = l->data[i + 1];
		l->nitems--;
	}

	return word;
}

/* 
 * Helper function for list_split(). Extract tokens from str.  
 * Return a pointer to the next token; at the same time, advance 
 * *str to point to the next separator.  
 *   sep (IN)	string containing list of separator characters
 *   str (IN)	double-pointer to string containing tokens and separators
 *   RETURN	next token
 */
static char *
next_tok(char *sep, char **str)
{
	char *tok;

	/* push str past any leading separators */
	while (**str != '\0' && strchr(sep, **str) != '\0')
		(*str)++;

	if (**str == '\0')
		return NULL;

	/* assign token pointer */
	tok = *str;

	/* push str past token and leave pointing to first separator */
	while (**str != '\0' && strchr(sep, **str) == '\0')
		(*str)++;

	/* nullify consecutive separators and push str beyond them */
	while (**str != '\0' && strchr(sep, **str) != '\0')
		*(*str)++ = '\0';

	return tok;
}

/*
 * Given a list of separators and a string, generate a list
 *   sep (IN)	string containing separater characters
 *   str (IN)	string containing tokens and separators
 *   RETURN 	new list containing all tokens
 */
list_t 
list_split(char *sep, char *str)
{
	list_t new = list_new();
	char *tok;

	if (sep == NULL)
		sep = SPACES;

	while ((tok = next_tok(sep, &str)) != NULL) {
		if (strlen(tok) > 0)
			list_push(new, tok);
	} 
			
	return new;
}


/* 
 * Given a list of seperators, a range operator, and a string, generate a list
 * 
 * sep(IN)  string containing seperator characters
 * r_op(IN) range operator character
 * RETURN   new list containing all tokens with ranges expanded
 */
list_t 
list_split_range(char *sep, char *r_op, char *str)
{
	char *tok, *cur; 
	int high, low, fmt;
	char prefix[256] = ""; 
	char buf[256]    = ""; 
	int pos          = 0;
	int error        = 0;
	int i            = 0;
	char range_op = r_op[0]; /* XXX support > 1 char range ops in future? */

	list_t new = list_new();

	/* return an empty list if an empty string was passed in */
	if (str == NULL || strlen(str) == 0)
		return new;

	/* remove characters to support Quadrics hostname ranges */
	for (cur = str; *cur != '\0'; cur++) {
		if (strchr("[]\n\t", *cur) == '\0')
			str[i++] = cur[0];
	}
	str[i] = '\0';

	while ((tok = next_tok(sep, &str)) != NULL) {

		/* save the current string for error messages */
		cur = tok;

		high = low = 0;

		/* find end of alpha part */
		/* do this by finding last occurence of range_op in str */
		pos = strlen(tok) - 1;
		if (strstr(tok, r_op) != '\0')
			while(pos >= 0 && (char)tok[pos--] != range_op) {;}

		/* now back up past any digits */
		while(pos >= 0 && isdigit((char)tok[--pos])) {;}

		pos++;

		/* create prefix string 
		 * if prefix will be zero length, but prefix already exists
		 * use the previous prefix and fmt
		 */
		if ((pos > 0) || (prefix[0] == '\0')) {
			memcpy(prefix, tok, (size_t) pos*sizeof(char));
			prefix[pos] = '\0';

			/* push pointer past prefix */
			tok += pos;

			/* count number of digits for ouput fmt */
			for (fmt=0; isdigit(tok[fmt]); ++fmt) {;}

			if (fmt == 0) 
				error = 1;

		} else
			tok += pos;

		/* get lower bound */
		low = strtoul(tok, (char**)&tok, 10);

		if (*tok == range_op) { /* now get range upper bound */
			/* push pointer past range op */
			++tok;

			/* find length of alpha part */
			for (pos=0; tok[pos] && !isdigit(tok[pos]); ++pos) {;}

			/* alpha part must match prefix or error
			 * this could mean we've got something like "rtr1-a2"
			 * so just record an error
			 */
			if (pos > 0) {
				if(pos != strlen(prefix) || strncmp(prefix, tok, pos) != 0)
					error = 1;
			}

			if (*tok != '\0')
				tok+=pos;

			/* make sure we have digits to the end */
			for(pos=0; tok[pos] && isdigit((char)tok[pos]); ++pos);

			if (pos > 0) { /* we have digits to process */
				high = strtoul(tok, (char**)&tok, 10);
			} else { /* bad boy, no digits */
				error = 1;
			}

			if ((low > high) || (high - low > MAX_RANGE))
				error = 1;

		} else {		/* single value */
			high = 0; 	/* special case, ugh. */
		}

		/* error if: 
		 * 1. we are not at end of string
		 * 2. upper bound equals lower bound
		 */
		if (*tok != '\0' || high == low) 
			error = 1;

		if (error) { /* assume this is not a range on any error */
			list_push(new, cur);
		} else {

			/* generate range and push elements onto list */
			do {
				snprintf(buf, 256, "%s%0*d", prefix, fmt, low);
				list_push(new, buf);
			} while (++low <= high);

		}

		error = 0;

	}

	return new;
}


/* 
 * Opposite of split (caller responsible for freeing result).  
 *   sep (IN)	string containing separater characters
 *   l (IN)	list
 */
char *
list_join(char *sep, list_t l)
{
	int i;
	char *result = NULL;

	assert(l->magic == LIST_MAGIC);
	for (i = 0; i < l->nitems; i++) {
		if (result != NULL)	/* add separator if not first token */
			xstrcat(&result, sep);
		xstrcat(&result, l->data[i]);
	}

	return result;
}	
	

/*
 * Dump a list, for debugging 
 *   l (IN)	list
 */
void 
list_dump(list_t l)
{
	int i;

	assert(l->magic == LIST_MAGIC);
	out("size   = %d\n", l->size);
	out("nitems = %d\n", l->nitems);
	for (i = 0; i < l->nitems; i++) 
		out("data[%d] = `%s'\n", i, l->data[i]);
}	

/* 
 * Push the contents of list l2 onto list l1.
 *   l1 (IN)	target list
 *   l2 (IN)	source list
 */
void 
list_pushl(list_t l1, list_t l2)
{
	int i;

	assert(l1->magic == LIST_MAGIC);
	assert(l2->magic == LIST_MAGIC);
	for (i = 0; i < l2->nitems; i++)
		list_push(l1, l2->data[i]);
}

/* 
 * Return true if item is found in list (not a substring--a complete match)
 *   l (IN)	list to be searched
 *   item (IN)	string to look for 
 */
int 
list_test(list_t l, char *item)
{
	int i, found = 0;

	assert(l->magic == LIST_MAGIC);
	for (i = 0; i < l->nitems; i++)	
		if (!strcmp(l->data[i], item)) {
			found = 1;
			break;	
		}
	return found;
}

/* 
 * Similar to list_pushl(), but only items not already found in l1 are pushed
 * from l2 to l1.
 *   l1 (IN)	target list
 *   l2 (IN)	source list
 */
void 
list_merge(list_t l1, list_t l2)
{
	int i;

	assert(l1->magic == LIST_MAGIC);
	assert(l2->magic == LIST_MAGIC);
	for (i = 0; i < l2->nitems; i++)
		if (!list_test(l1, l2->data[i]))
			list_push(l1, l2->data[i]);
}

/*
 * Return the number of items in a list.
 *   l (IN)	target list
 *   RETURN	number of items
 */
int 
list_length(list_t l)
{
	assert(l->magic == LIST_MAGIC);
	return l->nitems;
}

/* 
 * Return the nth element of a list.
 *   l (IN)	list
 *   n (IN)	index into list
 *   RETURN	pointer to element
 */
char *
list_nth(list_t l, int n)
{
	assert(l->magic == LIST_MAGIC);
	assert(n < l->nitems && n >= 0);
	return l->data[n];
}

/* 
 * Delete the nth element of the list (freeing deleted element).
 *   l (IN)	list
 *   n (IN)	index into list
 */
void 
list_del(list_t l, int n)
{
	int i;
	assert(l->magic == LIST_MAGIC);
	assert(n < l->nitems && n >= 0);
	Free((void **)&(l->data[n]));		/* free the string */
	for (i = n; i < l->nitems - 1; i++)
		l->data[i] = l->data[i + 1];
	l->nitems--;
}


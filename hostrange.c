/*
 * $Id$
 * $Source$
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
#include "hostrange.h"


/* 
 * Helper function for range_split(). Extract tokens from str.  
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
 * max size of hostname range in range_split()
 * (8k hosts enough?)
 */
#define MAX_RANGE	8192

/* 
 * Given a list of seperators, a range operator, and a string, generate a list
 * 
 * sep(IN)  string containing seperator characters
 * r_op(IN) range operator character
 * RETURN   new list containing all tokens with ranges expanded
 */
list_t 
range_split(char *sep, char *r_op, char *str)
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

char *
range_join(char *sep, char *r_op, list_t l)
{
	/* not implemented yet */
	return NULL;
}

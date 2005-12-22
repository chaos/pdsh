
#include <string.h>
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

static void _free_f (char *str)
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
    List new = list_create((ListDelF) _free_f);
    char *tok;

    if (sep == NULL)
        sep = " \t";

    while ((tok = _next_tok(sep, &str)) != NULL) {
        if (strlen(tok) > 0)
            list_append(new, Strdup(tok));
    }

    return new;
}



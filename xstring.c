/* 
 * $Id$ 
 *
 * Copyright (C) 2000 Regents of the University of California
 * See ./DISCLAIMER
 */

/*
 * Heap-oriented string functions.
 */

#include "conf.h"

#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <assert.h>

#include "xmalloc.h"
#include "xstring.h"

#define SPACES "\n\t "
 
#define XFGETS_CHUNKSIZE 128

/*
 * Zap leading and trailing occurrences of characters in 'verboten'.
 * (Note: don't pass in your base pointer or you won't be able to free!)
 *   str (IN/OUT)	pointer to string
 *   verboten (IN)	list of characters to be zapped (if NULL, zap spaces)
 */
void 
xstrcln(char **str, char *verboten)
{
	char *p;

	if (verboten == NULL)
		verboten = SPACES;

	/* move pointer past initial 'verboten' characters */
	while (*str != NULL && **str != '\0' && strchr(verboten, **str) != NULL)
		(*str)++;
	
	/* overwrite trailing 'verboten' characters with nulls */
	if (*str != NULL && strlen(*str) > 0) {
		p = *str + strlen(*str) - 1;
		while (p > *str && *p != '\0' && strchr(verboten, *p) != NULL)
			*p-- = '\0';
	}
}

/*
 * Similar to fgets(), but accepts a pointer to a dynamically allocated buffer
 * for the line and expands it as needed.  The buffer, if it has non-zero 
 * length, will always be terminated by a carriage return. 
 * EOF or error can be returned with valid data in the buffer.
 *   str (IN/OUT)	buffer where input is stored (xfgets may resize)
 *   size (IN/OUT)	current size of buffer
 *   stream (IN)	stream to read from
 *   RETURN		0 = EOF, -1 = error, 1 = connection still open.
 */
int 
xfgets(char **str, int *size, FILE *stream)
{
	int check_err = 0;
	int rv = 1;
	int nread = 0;

	/*
	 * Initialize buffer space if a pointer-to-null was passed in.
	 */	
	if (*str == NULL) {
		*str = xmalloc(XFGETS_CHUNKSIZE * sizeof(char));
		*size = XFGETS_CHUNKSIZE;
	}

	/*
	 * Read a line's worth of characters, or up to EOF or error.
	 * Expand buffer if necessary.
	 */
	do {
		/* allocate more buffer space if needed */
		if (nread == *size - 2) {
			*size += XFGETS_CHUNKSIZE;
			xrealloc((void **)str, *size);
		}
		/* read a character -- quit loop if we get EOF or error */
		if (fread(*str + nread, 1, 1, stream) != 1) {
			check_err = 1;
			break;
		}
		nread++;

	}  while (*(*str + nread - 1) != '\n');

	*(*str + nread) = '\0'; /* NULL termination */

	/*
	 * Determine if return value needs to be EOF (0) or error (-1).
	 */
	if (check_err) {
		if (feof(stream))
			rv = 0;
		else if (ferror(stream))
			rv = -1;

		/* add a terminating \n */
		if (strlen(*str) > 0)
			strcat(*str, "\n");
	}

	return rv;
}

/*
 * Same as above except it uses read() instead of fread().
 *   str (IN/OUT)	buffer where input is stored (xfgets may resize)
 *   size (IN/OUT)	current size of buffer
 *   fd (IN)		file descriptor to read from
 *   RETURN		0 = EOF, -1 = error, 1 = connection still open.
 */
int 
xfgets2(char **str, int *size, FILE *stream)
{
	int check_err = 0;
	int rv = 1;
	int nread = 0;
	int fd = fileno(stream);

	/*
	 * Initialize buffer space if a pointer-to-null was passed in.
	 */	
	if (*str == NULL) {
		*str = xmalloc(XFGETS_CHUNKSIZE * sizeof(char));
		*size = XFGETS_CHUNKSIZE;
	}

	/*
	 * Read a line's worth of characters, or up to EOF or error.
	 * Expand buffer if necessary.
	 */
	do {
		/* allocate more buffer space if needed */
		if (nread == *size - 2) {
			*size += XFGETS_CHUNKSIZE;
			xrealloc((void **)str, *size);
		}
		/* read a character -- quit loop if we get EOF or error */
		if ((rv = read(fd, *str + nread, 1)) != 1) {
			check_err = 1;
			break;
		}
		nread++;

	}  while (*(*str + nread - 1) != '\n');

	*(*str + nread) = '\0'; /* NULL termination */

	/*
	 * Determine if return value needs to be EOF (0) or error (-1).
	 */
	if (check_err) {
		/* add a terminating \n */
		if (strlen(*str) > 0)
			strcat(*str, "\n");
	}

	return rv;
}

/*
 * Ensure that a string has enough space to add 'needed' characters.
 * If the string is uninitialized, it should be NULL.
 */
static void 
makespace(char **str, int *size, int needed)
{
	int used;

	if (*str == NULL)
		*str = xmalloc(*size = needed + 1);
	else {
		used = strlen(*str) + 1;
		if (used + needed > *size) {
			xrealloc((void **)str, *size += XFGETS_CHUNKSIZE);
			makespace(str, size, needed);
		}
	}
}
		
/*
 * Duplicate a string.
 *   str (IN)		string to duplicate
 *   size (OUT)		size will be deposited here if non-NULL
 *   RETURN		copy of string
 */
char *
xstrdup(char *str, int *size)
{
	char *result = NULL;
	int mysize;

	makespace(&result, &mysize, strlen(str));
	strcpy(result, str);

	if (size != NULL)
		*size = mysize;

	return result;
}

/* 
 * Concatenate str2 onto str1, expanding str1 as needed.
 *   str1 (IN/OUT)	target string (pointer to in case of expansion)
 *   size (IN/OUT)	size of str1 (pointer to in case of expansion)
 *   str2 (IN)		source string
 */
void 
xstrcat(char **str1, int *size, char *str2)
{
	makespace(str1, size, strlen(str2));
	strcat(*str1, str2);
}

static void 
strcatchar(char *str, char c)
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
void 
xstrcatchar(char **str, int *size, char c)
{
	makespace(str, size, 1);
	strcatchar(*str, c);
}

void 
xstrerrorcat(char **buf, int *bufsize)
{
#if HAVE_STRERROR_R
        char errbuf[64];
        char *err = strerror_r(errno, errbuf, 64);
#elif HAVE_STRERROR
        char *err = strerror(errno);
#else
        extern char *sys_errlist[];
        char *err = sys_errlist[errno];
#endif
        xstrcat(buf, bufsize, err);
}


/* 
 * Replacement for libc basename
 *   path (IN)		path possibly containing '/' characters
 *   RETURN		last component of path
 */
char *
xbasename(char *path)
{
	char *p;

	p = strrchr(path , '/');
	return (p ? (p + 1) : path);
}	

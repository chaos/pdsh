/* 
 * $Id$  
 *
 * Copyright (C) 2000-2002 Regents of the University of California
 * See ./DISCLAIMER
 */

/*
 * Error printing routines with variable arguments.
 */

#if     HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdarg.h>
#include <netdb.h>
#include <string.h>
#include <assert.h>
#if	HAVE_UNISTD_H
#include <unistd.h>	/* gethostname */
#endif
#include <ctype.h>
#include <stdlib.h>	/* exit */

#include "xstring.h"
#include "xmalloc.h"
#include "dsh.h"

static char *prog = NULL;
static char *host = NULL;

/*
 * Call this before calling err() or errx().  Sets hostname and program name 
 * for %H, %p, and %P.
 *   str (IN)	program name
 */
void err_init(char *str)
{
        char thishost[MAXHOSTNAMELEN];
        char *p;
 
	gethostname(thishost, MAXHOSTNAMELEN);
	if ((p = strchr(thishost, '.')) != NULL)
		*p = '\0';
	host = Strdup(thishost);
	prog = Strdup(str);
}

/*
 * Free heap storage allocated by err_init()
 */
void err_cleanup()
{
	Free((void **)&prog);
	Free((void **)&host);
}

/* 
 * verr() is like vfprintf, but handles (only) the following formats:
 * following formats:
 * %s	string
 * %S   string, but treat as hostname and truncate after dot
 * %c	character
 * %m	string (sys_errlist[errno])
 * %d   int	
 * %z   equivalent to %.3d 
 * %p   program name with @host attached
 * %P   program name
 * %H   hostname for this host
 */
static void verr(FILE *stream, char *format, va_list ap) 
{
	char *buf = NULL;
	char *q;
	int percent = 0;
	char tmpstr[LINEBUFSIZE];

	assert(prog != NULL && host != NULL);
	while (format && *format) {			/* iterate thru chars */
		if (percent == 1) { 
			percent = 0;
			if (*format == 's') {		/* %s - string */
				xstrcat(&buf, va_arg(ap, char *));
			} else if (*format == 'S') {	/* %S - string, trunc */
				strcpy(tmpstr, va_arg(ap, char *));
				if (!isdigit(*tmpstr) 
						&& (q = strchr(tmpstr, '.')))
					*q = '\0';
				xstrcat(&buf, tmpstr);
			} else if (*format == 'z') {	/* %z - same as %.3d */
				sprintf(tmpstr, "%.3d", va_arg(ap, int));
				xstrcat(&buf, tmpstr);
			} else if (*format == 'c') {	/* %c - character */
				xstrcatchar(&buf, va_arg(ap, int));
			} else if (*format == 'd') {	/* %d - integer */
				sprintf(tmpstr, "%d", va_arg(ap, int));
				xstrcat(&buf, tmpstr);
			} else if (*format == 'm') {	/* %m - error code */
				xstrerrorcat(&buf);
			} else if (*format == 'P') {	/* %P - prog name */
				xstrcat(&buf, prog);
			} else if (*format == 'H') {	/* %H - this host */
				xstrcat(&buf, host);
			} else if (*format == 'p') {	/* %p - prog@host */
				sprintf(tmpstr, "%s@%s", prog, host);
				xstrcat(&buf, tmpstr);
			} else				/* pass thru */
				xstrcatchar(&buf, *format);
		} else if (*format == '%')
			percent = 1;			/* remember % */
		else
			xstrcatchar(&buf, *format);	/* pass thru */
		format++;
	}

	fputs(buf, stream);				/* print it */
	Free((void **)&buf);				/* clean up */
}

void err(char *format, ...)
{
	va_list	ap;

	va_start(ap, format);
	verr(stderr, format, ap);
	va_end(ap);
}

void errx(char *format, ...)
{
	va_list	ap;

	va_start(ap, format);
	verr(stderr, format, ap);
	va_end(ap);
	exit(1);
}

void out(char *format, ...)
{
	va_list	ap;

	va_start(ap, format);
	verr(stdout, format, ap);
	va_end(ap);
}

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
#include <unistd.h>             /* gethostname */
#endif
#include <ctype.h>
#include <stdlib.h>             /* exit */

#include "xstring.h"
#include "xmalloc.h"
#include "macros.h" 

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
void err_cleanup(void)
{
    Free((void **) &prog);
    Free((void **) &host);
}

/* 
 * _verr() is like vfprintf, but handles (only) the following formats:
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
static void _verr(FILE * stream, char *format, va_list ap)
{
    char *buf = NULL;
    char *q;
    int percent = 0;
    char tmpstr[LINEBUFSIZE];

    /* Note: snprintf silently truncates if argument exceeds size of tmpstr */

    assert(prog != NULL && host != NULL);
    while (format && *format) { /* iterate thru chars */
        if (percent == 1) {
            percent = 0;
            if (*format == 's') {       /* %s - string */
                xstrcat(&buf, va_arg(ap, char *));
            } else if (*format == 'S') {        /* %S - string, trunc */
                snprintf(tmpstr, sizeof(tmpstr), "%s", va_arg(ap, char *));
                if (!isdigit(*tmpstr)
                    && (q = strchr(tmpstr, '.')))
                    *q = '\0';
                xstrcat(&buf, tmpstr);
            } else if (*format == 'z') {        /* %z - same as %.3d */
                snprintf(tmpstr, sizeof(tmpstr), "%.3d", va_arg(ap, int));
                xstrcat(&buf, tmpstr);
            } else if (*format == 'c') {        /* %c - character */
                xstrcatchar(&buf, va_arg(ap, int));
            } else if (*format == 'd') {        /* %d - integer */
                snprintf(tmpstr, sizeof(tmpstr), "%d", va_arg(ap, int));
                xstrcat(&buf, tmpstr);
            } else if (*format == 'm') {        /* %m - error code */
                xstrerrorcat(&buf);
            } else if (*format == 'P') {        /* %P - prog name */
                assert(prog != NULL);
                xstrcat(&buf, prog);
            } else if (*format == 'H') {        /* %H - this host */
                assert(host != NULL);
                xstrcat(&buf, host);
            } else if (*format == 'p') {        /* %p - prog@host */
                assert(prog != NULL);
                assert(host != NULL);
                snprintf(tmpstr, sizeof(tmpstr), "%s@%s", prog, host);
                xstrcat(&buf, tmpstr);
            } else              /* pass thru */
                xstrcatchar(&buf, *format);
        } else if (*format == '%')
            percent = 1;        /* remember % */
        else
            xstrcatchar(&buf, *format); /* pass thru */
        format++;
    }

    fputs(buf, stream);         /* print it */
    Free((void **) &buf);       /* clean up */
}

void err(char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    _verr(stderr, format, ap);
    va_end(ap);
}

void errx(char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    _verr(stderr, format, ap);
    va_end(ap);
    exit(1);
}

void out(char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    _verr(stdout, format, ap);
    va_end(ap);
}

void errf(FILE *stream, char *format, va_list ap)
{
    if (!stream)
        return;

    _verr(stream, format, ap);
}

void lsd_fatal_error(char *file, int line, char *mesg)
{
    err ("%p: %s:%d: %s\n", file, line, mesg);
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

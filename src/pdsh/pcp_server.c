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
 * Copyright (c) 1983, 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

char copyright[] =
 "@(#) Copyright (c) 1983, 1990 The Regents of the University of California.\n"
 "All rights reserved.\n";

/*
 * From: @(#)rcp.c	5.32 (Berkeley) 2/25/91
 */
char rcsid[] = "$Id$";
/* #include "../version.h" */

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <sys/param.h>     /* roundup() */
#if HAVE_SYS_SYSMACROS_H
# include <sys/sysmacros.h>
#endif
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <pwd.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

#include "src/common/err.h"
#include "pcp_server.h"
#include "opt.h"

#ifndef roundup
#  define roundup(x, y) ((((x) + ((y) - 1)) / (y)) * (y))
#endif

/* The majority of the code below is unchanged from the original
 * rcp code.  Changes include:
 * - rcp bug fix
 * - removal of conditions that are impossible to hit in pdcp
 * - update error messages to use pdcp error functions
 * - minor changes to enhance readability and fit style to rest
 *   of pdsh/pdcp code. 
 * - pass infd/outfd through structure rather than global
 * - don't exit on error, just return
 */

typedef struct _buf {
    int	   cnt;
    char  *buf;
} BUF;

static int  _verifydir(struct pcp_server *s, const char *cp);
static int  _response(struct pcp_server *s);
static BUF *_allocbuf(struct pcp_server *s, BUF *bp, int fd, int blksize);
static void _error(struct pcp_server *s, const char *fmt, ...);
static void _sink(struct pcp_server *s, char *targ, BUF *bufp);

static int
_verifydir(struct pcp_server *s, const char *cp)
{
    struct stat stb;

    if (stat(cp, &stb) >= 0) {
        if ((stb.st_mode & S_IFMT) == S_IFDIR)
            return 0;
        errno = ENOTDIR;
    }
    _error(s, "%s not a directory\n", cp);
    return -1;
}

static int
_response(struct pcp_server *s)
{
    char resp;

    if (read(s->infd, &resp, sizeof(resp)) != sizeof(resp)) {
        _error(s, "lost connection\n");
        return -1;
    }

    switch(resp) {
        case 0:			/* ok */
            return 0;
        default:
            _error(s, "invalid response received\n");
            return -1;
    }

    /*NOTREACHED*/
    return 0;
}

static BUF *
_allocbuf(struct pcp_server *s, BUF *bp, int fd, int blksize)
{
    struct stat stb;
    int size;

    if (fstat(fd, &stb) < 0) {
        _error(s, "fstat: %m\n");
        return NULL;
    }

    size = roundup(stb.st_blksize, blksize);
    if (size == 0)
        size = blksize;
    if (bp->cnt < size) {
        if (bp->buf != 0)
            free(bp->buf);
        bp->buf = malloc(size);
        if (!bp->buf) {
            _error(s, "malloc: out of memory\n");
            return NULL;
        }
    }
    bp->cnt = size;
    return(bp);
}

static void
_error(struct pcp_server *s, const char *fmt, ...)
{
    static FILE *fp = NULL;
    char newfmt[1000];
    va_list ap;
    int save_errno = errno;   /* errno could be changed by fopen */

    if (!(fp = fdopen(s->outfd, "w")))
        return;

    va_start(ap, fmt);

    /* must put "1" at beginning of the format to indicate an error */
    snprintf(newfmt, 1000, "%c%s", 0x01, fmt);
    errno = save_errno;
    errf(fp, newfmt, ap);
    va_end(ap);

    fflush(fp);
}

static void
_sink(struct pcp_server *svr, char *targ, BUF *bufp) {
    register char *cp;
    struct stat stb;
    struct timeval tv[2];
    enum { YES, NO, DISPLAYED } wrerr;
    BUF *bp;
    off_t i, j, size;
    char ch;
    const char *why;
    int amt, count, exists, first, mask, mode;
    int ofd, setimes, targisdir, cursize = 0;
    char *np, buf[BUFSIZ], *namebuf = NULL;

#define	atime	tv[0]
#define	mtime	tv[1]
#define	SCREWUP(str)	{ why = str; goto screwup; }

    setimes = targisdir = 0;
    mask = umask(0);
    if (!svr->preserve)
        (void)umask(mask);

    if (svr->target_is_dir) {
        if (_verifydir(svr, svr->outfile) < 0)
            return;
    }

    (void)write(svr->outfd, "", 1);
    if (stat(targ, &stb) == 0 && (stb.st_mode & S_IFMT) == S_IFDIR)
        targisdir = 1;

    for (first = 1;; first = 0) {
		int rc;
        cp = buf;
        if ((rc = read(svr->infd, cp, 1)) <= 0) {
            if (namebuf)
                free(namebuf);
            return;
        }
        if (*cp++ == '\n')
            SCREWUP("unexpected <newline>");

        do {
            if (read(svr->infd, &ch, sizeof(ch)) != sizeof(ch))
                SCREWUP("lost connection");
            *cp++ = ch;
        } while (cp < &buf[BUFSIZ - 1] && ch != '\n');
        *cp = 0;

        if (buf[0] == '\01' || buf[0] == '\02') {
            if (buf[0] == '\02')
                goto end_server;
            continue;
        }

        if (buf[0] == 'E') {
            (void)write(svr->outfd, "", 1);
            if (namebuf)
                free(namebuf);
            return;
        }

        if (ch == '\n')
            *--cp = 0;

#define getnum(t) (t) = 0; while (isdigit(*cp)) (t) = (t) * 10 + (*cp++ - '0');
        cp = buf;
        if (*cp == 'T') {
            setimes++;
            cp++;
            getnum(mtime.tv_sec);
            if (*cp++ != ' ')
                SCREWUP("mtime.sec not delimited");
            getnum(mtime.tv_usec);
            if (*cp++ != ' ')
                SCREWUP("mtime.usec not delimited");
            getnum(atime.tv_sec);
            if (*cp++ != ' ')
                SCREWUP("atime.sec not delimited");
            getnum(atime.tv_usec);
            if (*cp++ != '\0')
                SCREWUP("atime.usec not delimited");
            (void)write(svr->outfd, "", 1);
            continue;
        }
        if (*cp != 'C' && *cp != 'D')
            SCREWUP("expected control record");

        mode = 0;
        for (++cp; cp < buf + 5; cp++) {
            if (*cp < '0' || *cp > '7')
                SCREWUP("bad mode");
            mode = (mode << 3) | (*cp - '0');
        }
        if (*cp++ != ' ')
            SCREWUP("mode not delimited");
        size = 0;
        while (isdigit(*cp))
            size = size * 10 + (*cp++ - '0');
        if (*cp++ != ' ')
            SCREWUP("size not delimited");

        /* filename is "retrieved" in this if/else block */
        if (targisdir) {

            /* achu: The original rcp code here was completely whack.
             * Memory was allocated for every file, memory was never
             * freed, cursize was never set to the current buffer
             * size, and a code path existed that could write past
             * allocated memory boundaries.  Lots and lots of fixes
             * here.  Atleast one person on google-groups concurs with
             * my thoughts.
             */

            int need;

            need = strlen(targ) + strlen(cp) + 250;
            if (need > cursize) {
                if (namebuf)
                    free(namebuf);
              
                if (!(namebuf = malloc(need))) { 
                    _error(svr, "out of memory\n");

                    /* original rcp may not work with a continue here,
                     * but it will work with pdcp protocol.
                     */
                    continue;
                }

                cursize = need;
            }
            (void)snprintf(namebuf, cursize, "%s%s%s", targ,
                           *targ ? "/" : "", cp);
            np = namebuf;
        }
        else
            np = targ;

        exists = stat(np, &stb) == 0;
        if (buf[0] == 'D') {
            if (exists) {
                if ((stb.st_mode & S_IFMT) != S_IFDIR) {
                    errno = ENOTDIR;
                    goto bad;
                }
                if (svr->preserve)
                    (void)chmod(np, mode);
            } else if (mkdir(np, mode) < 0)
                goto bad;

            /* recursively go down a directory */
            _sink(svr, np, bufp);

            if (setimes) {
                setimes = 0;
                if (utimes(np, tv) < 0)
                    _error(svr, "can't set times on %s: %m\n", np);
            }
            continue;
        }

        if ((ofd = open(np, O_WRONLY|O_CREAT, mode)) < 0) {
bad:	     
            _error(svr, "%s: %m\n", np);
            continue;
        }
        if (exists && svr->preserve)
            (void)fchmod(ofd, mode);

        (void)write(svr->outfd, "", 1);
        if ((bp = _allocbuf(svr, bufp, ofd, BUFSIZ)) == NULL) {
            (void)close(ofd);
            continue;
        }
        cp = bp->buf;
        count = 0;
        wrerr = NO;
        for (i = 0; i < size; i += BUFSIZ) {
            amt = BUFSIZ;
            if (i + amt > size)
                amt = size - i;
            count += amt;
            do {
                j = read(svr->infd, cp, amt);
                if (j <= 0) {
                    _error(svr, "%m\n");
                    goto end_server;
                }
                amt -= j;
                cp += j;
            } while (amt > 0);
            if (count == bp->cnt) {
                if (wrerr == NO && write(ofd, bp->buf, count) != count)
                    wrerr = YES;
                count = 0;
                cp = bp->buf;
            }
        }
        if (count != 0 && wrerr == NO && write(ofd, bp->buf, count) != count)
            wrerr = YES;
        if (ftruncate(ofd, size)) {
            _error(svr, "can't truncate %s: %m\n", np);
            wrerr = DISPLAYED;
        }
        (void)close(ofd);
        if (_response(svr) < 0)
            goto end_server;
        if (setimes && wrerr == NO) {
            setimes = 0;
            if (utimes(np, tv) < 0) {
                _error(svr, "can't set times on %s: %m\n", np);
                wrerr = DISPLAYED;
            }
        }
        switch(wrerr) {
            case YES:
                _error(svr, "%s: %m\n", np);
                break;
            case NO:
                (void)write(svr->outfd, "", 1);
                break;
            case DISPLAYED:
                break;
        }
    }

    return;

screwup:
    _error(svr, "protocol screwup: %s\n", why);

end_server:
    return;
}

int pcp_server(struct pcp_server *svr) 
{
	BUF buffer;
	memset (&buffer, 0, sizeof (buffer));

    /* If reverse copy, outfile is always a directory. */
    _sink (svr, svr->outfile, &buffer);

	if (buffer.buf)
		free (buffer.buf);
    return 0;
}

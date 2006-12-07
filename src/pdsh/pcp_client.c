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

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <sys/param.h>     /* roundup() */
#if HAVE_SYS_SYSMACROS_H
# include <sys/sysmacros.h>
#endif
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "src/common/err.h"
#include "src/common/fd.h"
#include "src/common/list.h"
#include "src/common/xstring.h"
#include "src/common/err.h"
#include "src/common/xmalloc.h"
#include "pcp_client.h"
#include "wcoll.h"

#ifndef MAXPATHNAMELEN
#define MAXPATHNAMELEN MAXPATHLEN
#endif


static void _rexpand_dir(List list, char *name)
{
    DIR *dir;
    struct dirent *dp;
    struct stat sb;
    char file[MAXPATHNAMELEN];
    struct pcp_filename *pf = NULL;

    dir = opendir(name);
    if (dir == NULL)
        errx("%p: opendir: %s: %m\n", name);
    while ((dp = readdir(dir))) {
        if (dp->d_ino == 0)
            continue;
        if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, ".."))
            continue;
        snprintf(file, sizeof(file), "%s/%s", name, dp->d_name);
        if (stat(file, &sb) < 0)
            errx("%p: can't stat %s: %m\n", file);
        if (access(name, R_OK) < 0)
            errx("%p: access: %s: %m\n", name);
        if (!S_ISDIR(sb.st_mode) && !S_ISREG(sb.st_mode))
            errx("%p: not a regular file or directory: %s\n", file);

        /* XXX: This memleaks */
        pf = Malloc(sizeof(struct pcp_filename));
        pf->filename = Strdup(file);
        pf->file_specified_by_user = 0;

        list_append(list, pf);
        if (S_ISDIR(sb.st_mode))
            _rexpand_dir(list, file);
    }
    closedir(dir);

    /* Since pdcp reads file names and directories only once for
     * efficiency, we must specify a special flag so we know when
     * to tell the server to "move up" the directory tree.
     */
    
    /* XXX: This memleaks */
    pf = Malloc(sizeof(struct pcp_filename));
    pf->filename = Strdup(EXIT_SUBDIR_FILENAME);
    pf->file_specified_by_user = 0;
    list_append(list, pf);
}

List pcp_expand_dirs(List infiles)
{
    List new = list_create(NULL);
    struct stat sb;
    char *name;
    ListIterator i;

    i = list_iterator_create(infiles);
    while ((name = list_next(i))) {
        struct pcp_filename *pf = NULL;

        if (access(name, R_OK) < 0)
            errx("%p: access: %s: %m\n", name);
        if (stat(name, &sb) < 0)
            errx("%p: stat: %s: %m\n", name);
        
        /* XXX: This memleaks */
        pf = Malloc(sizeof(struct pcp_filename));
        pf->filename = name;
        pf->file_specified_by_user = 1;

        list_append(new, pf);

        /* -r option checked during command line argument checks */
        if (S_ISDIR(sb.st_mode))
            _rexpand_dir(new, name);
    }
    
    return new;
}

/*
 * Wrapper for the write system call that handles short writes.
 * Not sure if write ever returns short in practice but we have to be sure.
 *	fd (IN)		file descriptor to write to 
 *	buf (IN)	data to write
 *	size (IN)	size of buf
 *	RETURN		-1 on failure, size on success
 */
static int _pcp_write(int fd, char *buf, int size)
{
    char *bufp = buf;
    int towrite = size;
    int outbytes;

    while (towrite > 0) {
        outbytes = write(fd, bufp, towrite);
        if (outbytes <= 0) {
            assert(outbytes != 0);
            return -1;
        }
        towrite -= outbytes;
        bufp += outbytes;
    }
    return size;
}

/*
 * Write the contents of the named file to the specified file descriptor.
 *	outfd (IN)	file descriptor to write to 
 *	filename (IN)	name of file
 *	host (IN)	name of remote host for error messages
 *	RETURN		-1 on failure, 0 on success.
 */
static int _pcp_send_file_data(int outfd, char *filename, char *host)
{
    int filefd, inbytes, total = 0;
    char tmpbuf[BUFSIZ];

    filefd = open(filename, O_RDONLY);
    /* checked ahead of time - shouldn't happen */
    if (filefd < 0) {
        err("%S: _pcp_send_file_data: open %s: %m\n", host, filename);
        return -1;
    }
    do {
        inbytes = read(filefd, tmpbuf, BUFSIZ);
        if (inbytes < 0) {
            err("%S: _pcp_send_file_data: read %s: %m\n", host, filename);
            return -1;
        }
        if (inbytes > 0) {
            total += inbytes;
            if (_pcp_write(outfd, tmpbuf, inbytes) < 0) {
                err("%S: _pcp_send_file_data: write: %m\n", host);
                return -1;
            }
        }
    } while (inbytes > 0);      /* until EOF */
    close(filefd);
    return 0;
}

/*
 * Send string to the specified file descriptor.  Do not send trailing '\0'
 * as RCP terminates strings with newlines.
 *	fd (IN)		file descriptor to write to
 *	str (IN)	string to write
 *	host (IN)	name of remote host for error messages
 *	RETURN 		-1 on failure, 0 on success
 */
static int pcp_sendstr(int outfd, char *str, char *host)
{
    int n;
    assert(strlen(str) > 0);
    assert(str[strlen(str) - 1] == '\n');

    if ((n = _pcp_write(outfd, str, strlen(str))) < 0) 
        return -1;

    assert(n == strlen(str));
    return 0;
}

/*
 * Receive an RCP response code and possibly error message.
 *	fd (IN)		file desciptor to read from
 *	host (IN)	hostname for error messages
 *	RETURN		-1 on fatal error, 0 otherwise
 */
static int pcp_response(int infd, char *host)
{
    char resp;
    int i = 0, result = -1;
    int n;
    char errstr[BUFSIZ];

    if ((n = read(infd, &resp, sizeof(resp))) != sizeof(resp)) 
        return (-1);

    switch (resp) {
        case 0:                /* ok */
            result = 0;
            break;
        default:               /* just error string */
            errstr[i++] = resp;
            result = 0;
        case 1:                /* fatal error + string */
            fd_read_line (infd, &errstr[i], BUFSIZ - i);
            err("%p: %S: %s: %s", host, result ? "fatal" : "error", errstr);
            break;
    }
    return result;
}

#define RCP_MODEMASK (S_ISUID|S_ISGID|S_ISVTX|S_IRWXU|S_IRWXG|S_IRWXO)

int pcp_sendfile(struct pcp_client *pcp, char *file, char *output_file)
{
    int result = 0;
    char tmpstr[BUFSIZ], *template;
    struct stat sb;

	if (output_file == NULL)
		output_file = file;

    /*err("%S: %s\n", host, file); */

    if (stat(file, &sb) < 0) {
        err("%S: %s: %m\n", pcp->host, file);
        goto fail;
    }

    if (pcp->preserve) {
        /* 
         * 1: SEND stat time: "T%ld %ld %ld %ld\n" 
         *    (st_mtime, st_mtime_usec, st_atime, st_atime_usec)
         */
        snprintf(tmpstr, sizeof(tmpstr), "T%ld %ld %ld %ld\n",
                 (long) sb.st_mtime, 0L, sb.st_atime, 0L);
        if (pcp_sendstr(pcp->outfd, tmpstr, pcp->host) < 0)
            goto fail;

        /* 2: RECV response code */
        if (pcp_response(pcp->infd, pcp->host) < 0)
            goto fail;
    }

    if (S_ISDIR(sb.st_mode)) {
        /* 
         * 3a: SEND directory mode: "D%04o %d %s\n"
         *     (st_mode & RCP_MODEMASK, 0, name)
         */
        snprintf(tmpstr, sizeof(tmpstr), "D%04o %d %s\n",
                 sb.st_mode & RCP_MODEMASK, 0, xbasename(output_file));
        if (pcp_sendstr(pcp->outfd, tmpstr, pcp->host) < 0)
            goto fail;
    } else {
        /* 
         * 3b: SEND file mode: "C%04o %lld %s\n" or "C%04o %ld %s\n"
         *    (st_mode & MODE_MASK, st_size, basename(filename))
         *    Use second template if sizeof(st_size) > sizeof(long).
         */
        template = (sizeof(sb.st_size) > sizeof(long)
                    ? "C%04o %lld %s\n" : "C%04o %ld %s\n");
        snprintf(tmpstr, sizeof(tmpstr), template,
                 sb.st_mode & RCP_MODEMASK, sb.st_size, xbasename(output_file));
        if (pcp_sendstr(pcp->outfd, tmpstr, pcp->host) < 0)
            goto fail;
    }

    /* 4: RECV response code */
    if (pcp_response(pcp->infd, pcp->host) < 0)
        goto fail;

    if (S_ISREG(sb.st_mode)) {
        /* 5: SEND data */
        if (_pcp_send_file_data(pcp->outfd, file, pcp->host) < 0)
            goto fail;

        /* 6: SEND NULL byte */
        if (_pcp_write(pcp->outfd, "", 1) < 0)
            goto fail;

        /* 7: RECV response code */
        if (pcp_response(pcp->infd, pcp->host) < 0)
            goto fail;
    }

    result = 1;                 /* indicate success */
  fail:
    return result;
}

static int _pcp_sendfile (struct pcp_filename *pf, struct pcp_client *pcp)
{
	char *output_filename = NULL;

	if (strcmp(pf->filename, EXIT_SUBDIR_FILENAME) == 0) {
		if (pcp_sendstr(pcp->outfd, EXIT_SUBDIR_FLAG, pcp->host) < 0)
			errx("%p: failed to send exit subdir flag\n");
		if (pcp_response(pcp->infd, pcp->host) < 0)
			errx("%p: failed to exit subdir properly\n");
		return (-1);
	}

	/* during a reverse copy, the hostname has to be attached
	 * to the end of the output filename for files specified
	 * by the user.
	 */
	if (pcp->pcp_client && pf->file_specified_by_user) {
		output_filename = Strdup(pf->filename);
		xstrcat(&output_filename, ".");
		xstrcat(&output_filename, pcp->host);
	}

	pcp_sendfile (pcp, pf->filename, output_filename);

	return (0);
}

int pcp_client(struct pcp_client *pcp)
{
    /* 0: RECV response code */
    if (pcp_response(pcp->infd, pcp->host) >= 0) {
        /* Send the files */
		list_for_each (pcp->infiles, (ListForF) _pcp_sendfile, pcp);
        return 0;
    }
    return -1;
}

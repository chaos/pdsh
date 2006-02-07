/*****************************************************************************\
 *  $Id$
 *****************************************************************************
 *  Copyright (C) 2001-2002 The Regents of the University of California.
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
 * This code is based on the BSD rcmd.c with MT safety added, and the 
 * interface changed.  Original UC regents header included below.
 */

/*
 * Copyright (c) 1983, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
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

/*
 * Changes:
 *  - MT save 
 *  - changed functional interface.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)rcmd.c	8.3 (Berkeley) 3/26/94";
#endif                          /* LIBC_SCCS and not lint */

#if     HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/param.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <stdio.h>
#if	HAVE_PTHREAD_H
#include <pthread.h>
#endif
#include <netinet/in.h>
#include <arpa/inet.h>

#include <signal.h>
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <netdb.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <pwd.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#if HAVE_STRINGS_H
#include <strings.h>            /* AIX FD_SET calls bzero */
#endif

#include "src/common/err.h"
#include "src/common/list.h"
#include "src/common/xpoll.h"
#include "src/pdsh/dsh.h"
#include "src/pdsh/mod.h"

#define RSH_PORT 514

#if STATIC_MODULES
#  define pdsh_module_info xrcmd_module_info
#  define pdsh_module_priority xrcmd_module_priority
#endif    

int pdsh_module_priority = DEFAULT_MODULE_PRIORITY;

static int xrcmd_init(opt_t *);
static int xrcmd_signal(int, void *, int);
static int xrcmd(char *, char *, char *, char *, char *, int, int *, void **); 

/* 
 * Export pdsh module operations structure
 */
struct pdsh_module_operations xrcmd_module_ops = {
  (ModInitF)       NULL, 
  (ModExitF)       NULL, 
  (ModReadWcollF)  NULL,
  (ModPostOpF)     NULL,
};

/*
 *  Export rcmd module operations
 */
struct pdsh_rcmd_operations xrcmd_rcmd_ops = {
    (RcmdInitF)  xrcmd_init,
    (RcmdSigF)   xrcmd_signal,
    (RcmdF)      xrcmd,
};

/* 
 * Export module options
 */
struct pdsh_module_option xrcmd_module_options[] = 
 { 
   PDSH_OPT_TABLE_END
 };

/* 
 * Xrcmd module info 
 */
struct pdsh_module pdsh_module_info = {
  "rcmd",
  "rsh",
  "Jim Garlick <garlick@llnl.gov>",
  "BSD rcmd connect method",
  DSH | PCP, 

  &xrcmd_module_ops,
  &xrcmd_rcmd_ops,
  &xrcmd_module_options[0],
};

static int xrcmd_init(opt_t * opt)
{
    /* not implemented */
    return 0;
}

/*
 * Use rcmd backchannel to propagate signals.
 * 	efd (IN)	file descriptor connected socket (-1 if not used)
 *	signum (IN)	signal number to send
 */
static int xrcmd_signal(int efd, void *arg, int signum)
{
    char c;

    if (efd >= 0) {
        /* set non-blocking mode for write - just take our best shot */
        if (fcntl(efd, F_SETFL, O_NONBLOCK) < 0)
            err("%p: fcntl: %m\n");
        c = (char) signum;
        write(efd, &c, 1);
    }
    return 0;
}

/*
 * The rcmd call itself.
 * 	ahost (IN)	remote hostname
 *	addr (IN)	4 byte internet address
 *	locuser (IN)	local username
 *	remuser (IN)	remote username
 *	cmd (IN)	command to execute
 *	rank (IN)	MPI rank for this process
 *	fd2p (IN/OUT)	if non-NULL, open stderr backchannel on this fd
 *	s (RETURN)	socket for stdout/sdin or -1 on failure
 */
static int
xrcmd(char *ahost, char *addr, char *locuser, char *remuser,
      char *cmd, int rank, int *fd2p, void **arg)
{
    struct sockaddr_in sin, from;
    sigset_t oldset, blockme;
    pid_t pid;
    int s, lport, timo, rv;
    char c;
    struct xpollfd xpfds[2];

    pid = getpid();

    sigemptyset(&blockme);
    sigaddset(&blockme, SIGURG);
    pthread_sigmask(SIG_BLOCK, &blockme, &oldset);
    for (timo = 1, lport = IPPORT_RESERVED - 1;;) {
        s = rresvport(&lport);
        if (s < 0) {
            if (errno == EAGAIN)
                err("%p: %S: rcmd: socket: all ports in use\n", ahost);
            else
                err("%p: %S: rcmd: socket: %m\n", ahost);
            pthread_sigmask(SIG_SETMASK, &oldset, NULL);
            return (-1);
        }
        fcntl(s, F_SETOWN, pid);
        sin.sin_family = AF_INET;
        memcpy(&sin.sin_addr, addr, IP_ADDR_LEN);
        sin.sin_port = htons(RSH_PORT);
        rv = connect(s, (struct sockaddr *) &sin, sizeof(sin));
        if (rv >= 0)
            break;
        (void) close(s);
        if (errno == EADDRINUSE) {
            lport--;
            continue;
        }
        if (errno == ECONNREFUSED && timo <= 16) {
            (void) sleep(timo);
            timo *= 2;
            continue;
        }
        if (errno == EINTR)
            err("%p: %S: connect: timed out\n", ahost);
        else
            err("%p: %S: connect: %m\n", ahost);
        pthread_sigmask(SIG_SETMASK, &oldset, NULL);
        return (-1);
    }
    lport--;
    if (fd2p == 0) {
        write(s, "", 1);
        lport = 0;
    } else {
        char num[8];
        int s2 = rresvport(&lport), s3;
        socklen_t len = sizeof(from);   /* arg to accept */

        if (s2 < 0)
            goto bad;
        listen(s2, 1);
        snprintf(num, sizeof(num), "%d", lport);
        if (write(s, num, strlen(num) + 1) != strlen(num) + 1) {
            err("%p: %S: rcmd: write (setting up stderr): %m\n", ahost);
            (void) close(s2);
            goto bad;
        }
        errno = 0;
        xpfds[0].fd = s;
        xpfds[1].fd = s2;
        xpfds[0].events = xpfds[1].events = XPOLLREAD;
        if (((rv = xpoll(xpfds, 2, -1)) < 0) || rv != 1 || (xpfds[0].revents > 0)) {
          if (errno != 0)
            err("%p: %S: rcmd: xpoll (setting up stderr): %m\n", ahost);
          else
            err("%p: %S: rcmd: xpoll: protocol failure in circuit setup\n", ahost);
          (void) close(s2);
          goto bad;
        }
        s3 = accept(s2, (struct sockaddr *) &from, &len);
        (void) close(s2);
        if (s3 < 0) {
            err("%p: %S: rcmd: accept: %m\n", ahost);
            lport = 0;
            goto bad;
        }
        *fd2p = s3;
        from.sin_port = ntohs((u_short) from.sin_port);
        if (from.sin_family != AF_INET ||
            from.sin_port >= IPPORT_RESERVED ||
            from.sin_port < IPPORT_RESERVED / 2) {
            err("%p: %S: socket: protocol failure in circuit setup\n",
                ahost);
            goto bad2;
        }
    }
    (void) write(s, locuser, strlen(locuser) + 1);
    (void) write(s, remuser, strlen(remuser) + 1);
    (void) write(s, cmd, strlen(cmd) + 1);
    rv = read(s, &c, 1);
    if (rv < 0) {
        if (errno == EINTR)
            err("%p: %S: read: protocol failure: %s\n",
                ahost, "timed out");
        else
            err("%p: %S: read: protocol failure: %m\n", ahost);
        goto bad2;
    } else if (rv != 1) {
        err("%p: %S: read: protocol failure: %s\n",
            ahost, "invalid response");
        goto bad2;
    }
    if (c != 0) {
        /* retrieve error string from remote server */
        char tmpbuf[LINEBUFSIZE];
        char *p = tmpbuf;

        while (read(s, &c, 1) == 1) {
            *p++ = c;
            if (c == '\n')
                break;
        }
        if (c != '\n')
            *p++ = '\n';
        *p++ = '\0';
        err("%S: %s", ahost, tmpbuf);
        goto bad2;
    }
    pthread_sigmask(SIG_SETMASK, &oldset, NULL);
    return (s);
  bad2:
    if (lport)
        (void) close(*fd2p);
  bad:
    (void) close(s);
    pthread_sigmask(SIG_SETMASK, &oldset, NULL);
    return (-1);
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

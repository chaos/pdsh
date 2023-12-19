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
 * This code is based on the MIT Kerberos IV kcmd.c with some athena hacks
 * removed and MT safety added, and the interface changed.  Original UC regents
 * header from BSD included below.
 *
 * XXX for some reason it takes a really long time (several seconds per node)
 * to acquire service tickets!  Why?
 */

/*
 * Copyright (c) 1983 Regents of the University of California. All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms are permitted provided that
 * the above copyright notice and this paragraph are duplicated in all such
 * forms and that any documentation, advertising materials, and other
 * materials related to such distribution and use acknowledge that the
 * software was developed by the University of California, Berkeley.  The
 * name of the University may not be used to endorse or promote products
 * derived from this software without specific prior written permission. THIS
 * SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#if     HAVE_CONFIG_H
#include "config.h"
#endif

#if	HAVE_PTHREAD_H
#include <pthread.h>
#endif

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <ctype.h>
#include <pwd.h>
#include <sys/param.h>
#if HAVE_SYS_FILE_H
#include <sys/file.h>
#endif
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <netinet/in.h>

#include <netdb.h>
#include <errno.h>
#include <krb.h>
#include <kparse.h>
#include <fcntl.h>              /* for F_SETOWN */

#include "src/common/xmalloc.h"
#include "src/common/xstring.h"
#include "src/common/macros.h"
#include "src/common/err.h"
#include "src/common/list.h"
#include "src/common/xpoll.h"
#include "src/pdsh/privsep.h"

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 64
#endif

#define KCMD_PORT 544

#if STATIC_MODULES
#  define pdsh_module_info k4cmd_module_info
#  define pdsh_module_priority k4cmd_module_priority
#endif

int pdsh_module_priority = DEFAULT_MODULE_PRIORITY;

extern errno;
extern char *inet_ntoa();

static int k4cmd_init(opt_t *);
static int k4cmd_signal(int, void *, int);
static int k4cmd(char *, char *, char *, char *, char *, int, int *, void **);

/*
 * Export pdsh module operations structure
 */
struct pdsh_module_operations k4cmd_module_ops = {
    (ModInitF)       NULL,
    (ModExitF)       NULL,
    (ModReadWcollF)  NULL,
    (ModPostOpF)     NULL,
};

/*
 *  Export rcmd module operations
 */
struct pdsh_rcmd_operations k4cmd_rcmd_ops = {
    (RcmdInitF)  k4cmd_init,
    (RcmdSigF)   k4cmd_signal,
    (RcmdF)      k4cmd,
};

/*
 * Export module options
 */
struct pdsh_module_option k4cmd_module_options[] =
 {
   PDSH_OPT_TABLE_END
 };

/*
 * k4cmd module info
 */
struct pdsh_module pdsh_module_info = {
    "rcmd",
    "k4",
    "Jim Garlick <garlick@llnl.gov>",
    "kerberos based rcmd connect method",
    DSH | PCP,

    &k4cmd_module_ops,
    &k4cmd_rcmd_ops,
    &k4cmd_module_options[0],
};

static int k4cmd_init(opt_t * opt)
{
    /* not implemented */
    return 0;
}

/*
 * Use rcmd backchannel to propagate signals.
 *      efd (IN)        file descriptor connected socket (-1 if not used)
 *      signum (IN)     signal number to send
 */
static int k4cmd_signal(int efd, void *arg, int signum)
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
 *      ahost (IN)      remote hostname
 *	addr (IN)	IP address
 *      locuser (IN)    local username
 *      remuser (IN)    remote username
 *      cmd (IN)        command to execute
 *       rank (IN)	MPI rank
 *      fd2p (IN/OUT)   if non-NULL, open stderr backchannel on this fd
 *      s (RETURN)      socket for stdout/sdin or -1 on failure
 */
static int
k4cmd(char *ahost, char *addr, char *locuser, char *remuser, char *cmd,
      int rank, int *fd2p, void **arg)
{
    KTEXT_ST ticket;            /* kerberos IV context */
    CREDENTIALS cred;
    Key_schedule schedule;
    MSG_DAT msg_data;
    struct sockaddr_in faddr;
    struct sockaddr_in laddr;

    int s, pid;
    sigset_t oldset, blockme;
    struct sockaddr_in sin, from;
    char c;
    int lport = IPPORT_RESERVED - 1;
    unsigned long krb_options = 0L;
    static pthread_mutex_t mylock = PTHREAD_MUTEX_INITIALIZER;
    int status;
    int rc, rv;
    struct xpollfd xpfds[2];

    pid = getpid();

    sigemptyset(&blockme);
    sigaddset(&blockme, SIGURG);
    pthread_sigmask(SIG_BLOCK, &blockme, &oldset);
    for (;;) {
        s = privsep_rresvport(&lport);
        if (s < 0) {
            if (errno == EAGAIN)
                err("%p: %S: socket: All ports in use\n", ahost);
            else
                err("%p: %S: k4cmd: socket: %m\n", ahost);
            pthread_sigmask(SIG_SETMASK, &oldset, NULL);
            return (-1);
        }
        fcntl(s, F_SETOWN, pid);
        sin.sin_family = AF_INET;
        memcpy((caddr_t) & sin.sin_addr, addr, IP_ADDR_LEN);
        sin.sin_port = htons(KCMD_PORT);

        rv = connect(s, (struct sockaddr *) &sin, sizeof(sin));

        if (rv >= 0)
            break;
        (void) close(s);
        if (errno == EADDRINUSE) {
            lport--;
            continue;
        }
        if (errno == EINTR)
            err("%p: %S: connect timed out\n", ahost);
        else
            err("%p: %S: %m\n", ahost);
        pthread_sigmask(SIG_SETMASK, &oldset, NULL);
        return (-1);
    }
    if (fd2p == 0) {
        write(s, "", 1);
        lport = 0;
    } else {
        char num[8];
        int s2, s3;
        socklen_t len = sizeof(from);

        s2 = privsep_rresvport(&lport);
        if (s2 < 0) {
            goto bad;
        }
        listen(s2, 1);
        (void) snprintf(num, sizeof(num), "%d", lport);
        if (write(s, num, strlen(num) + 1) != strlen(num) + 1) {
            err("%p: %S: write: setting up stderr: %m\n", ahost);
            (void) close(s2);
            goto bad;
        }
        errno = 0;
        xpfds[0].fd = s;
        xpfds[1].fd = s2;
        xpfds[0].events = xpfds[1].events = XPOLLREAD;
        if (((rv = xpoll(xpfds, 2, -1)) < 0) || rv != 1 || (xpfds[0].revents > 0)) {
          if (errno != 0)
            err("%p: %S: k4cmd: xpoll (setting up stderr): %m\n", ahost);
          else
            err("%p: %S: k4cmd: xpoll: protocol failure in circuit setup\n", ahost);
          (void) close(s2);
          goto bad;
        }
        s3 = accept(s2, (struct sockaddr *) &from, &len);
        (void) close(s2);
        if (s3 < 0) {
            err("%p: %S: accept: %m\n", ahost);
            lport = 0;
            goto bad;
        }
        *fd2p = s3;
        from.sin_port = ntohs((u_short) from.sin_port);
    }

    /*
     * Kerberos-authenticated service.  Don't have to send locuser, since
     * its already in the ticket, and we'll extract it on the other side.
     */
    /*krb_options |= KOPT_DONT_CANON; */
    pthread_mutex_lock(&mylock);
    status = krb_sendauth(krb_options, s, &ticket, "rcmd", ahost,
                          NULL, (unsigned long) pid, &msg_data,
                          &cred, schedule, &laddr, &faddr, "KCMDV0.1");

    if (status != KSUCCESS) {
        /*
         * this part involves some very intimate knowledge of a
         * particular sendauth implementation to pry out the old
         * bits. This only catches the case of total failure -- but
         * that's the one where we get useful data from the remote
         * end. If we even get an authenticator back, then the
         * problem gets diagnosed locally anyhow.
         */
        extern KRB_INT32 __krb_sendauth_hidden_tkt_len;
        char *old_data = (char *) &__krb_sendauth_hidden_tkt_len;
        char tmpbuf[LINEBUFSIZE];
        char *p = tmpbuf;

        if ((status == KFAILURE) && (*old_data == 1)) {
            strncpy(tmpbuf, old_data + 1, 3);
            tmpbuf[3] = '\0';
            err("%p: %S: %s", ahost, tmpbuf);
            *old_data = (-1);
        }
        if ((status == KFAILURE) && (*old_data == (char) -1)) {
            while (read(s, &c, 1) == 1) {
                /*(void) write(2, &c, 1); */
                *p++ = c;
                if (c == '\n')
                    break;
            }
            *p++ = '\0';
            err("%p: %S: %s", ahost, tmpbuf);
            status = -1;
        }
        switch (status) {
        case KDC_PR_UNKNOWN:
            err("%p: %S: not registered for kerberos\n", ahost);
            break;
        case NO_TKT_FIL:
            err("%p: %S: no tickets file found\n", ahost);
            break;
        default:
            err("%p: %S: k4cmd failed: %s\n", ahost,
                (status == -1) ? "k4cmd protocol failure" :
                krb_get_err_text(status));
        }
        pthread_mutex_unlock(&mylock);
        goto bad2;
    }
    pthread_mutex_unlock(&mylock);
    (void) write(s, remuser, strlen(remuser) + 1);
    (void) write(s, cmd, strlen(cmd) + 1);

    if ((rc = read(s, &c, 1)) != 1) {
        if (rc == -1) {
            err("%p: %S: read: %m\n", ahost);
        } else {
            err("%p: %S: k4cmd: bad connection with remote host\n", ahost);
        }
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

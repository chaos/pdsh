/* 
 * $Id$  
 */


/*
 * Started with BSD rcmd.c.
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

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)rcmd.c	8.3 (Berkeley) 3/26/94";
#endif /* LIBC_SCCS and not lint */

#if     HAVE_CONFIG_H
#include "config.h"
#endif

#if	HAVE_ELAN

#include <sys/param.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <elan3/elanvp.h>

#include "xmalloc.h"
#include "xstring.h"
#include "list.h"
#include "qswutil.h"
#include "err.h"

#include "dsh.h"	/* LINEBUFSIZE */


#define QSHELL_PORT 523

#if HAVE_GETHOSTBYNAME_R
#define HBUF_LEN	1024
#endif

extern char **environ;

static char 		cwd[MAXPATHLEN+1];
static ELAN_CAPABILITY	cap;
static qsw_info_t	qinfo;

/*
 * Use rcmd backchannel to propagate signals.
 *      efd (IN)        file descriptor connected socket (-1 if not used)
 *      signum (IN)     signal number to send
 */
void
qcmd_signal(int efd, int signum)
{
        char c;

        if (efd >= 0) {
		/* set non-blocking mode for write - just take our best shot */
		if (fcntl(efd, F_SETFL, O_NONBLOCK) < 0)
			err("%p: fcntl: %m\n");
                c = (char)signum;
                write(efd, &c, 1);
        }
}

/* 
 * Intialize elan capability and info structures that will be used when
 * running the job.
 * 	wcoll (IN)	list of nodes
 */
void
qcmd_init(opt_t *opt)
{
	int totprocs = opt->nprocs * list_length(opt->wcoll);

	if (getcwd(cwd, sizeof(cwd)) == NULL)	/* cache working directory */
		errx("%p: getcwd failed\n");

	/* initialize Elan capability structure. */
	if (qsw_init_capability(&cap, totprocs, opt->wcoll, 
				(opt->q_allocation == ALLOC_CYCLIC)) < 0)
		errx("%p: failed to initialize Elan capability\n");

	/* initialize elan info structure */
	qinfo.prgnum = qsw_get_prgnum(); /* call after qsw_init_capability */
	qinfo.nnodes = list_length(opt->wcoll);
	qinfo.nprocs = totprocs;
	qinfo.nodeid = qinfo.procid = qinfo.rank = 0;
}

/*
 * Send extra arguments to qshell server
 *	s (IN)		socket 
 *	nodeid (IN)	node index for this connection
 */
static int
qcmd_send_extra_args(int s, int nodeid)
{
	char **ep;
	char tmpstr[1024];
	int count = 0;

	/* send current working dir */
	(void)write(s, cwd, strlen(cwd)+1);

	/* send environment (count followed by variables, each \0-term) */
	for (ep = environ; *ep != NULL; ep++)
		count++;
	sprintf(tmpstr, "%d", count);
	(void)write(s, tmpstr, strlen(tmpstr)+1);
	for (ep = environ; *ep != NULL; ep++)	
		(void)write(s, *ep, strlen(*ep)+1);

	/* send elan capability */
	if (qsw_encode_cap(tmpstr, sizeof(tmpstr), &cap) < 0)
		return -1;
	(void)write(s, tmpstr, strlen(tmpstr)+1);

	/* send elan info */
	qinfo.nodeid = qinfo.rank = qinfo.procid = nodeid;
	if (qsw_encode_info(tmpstr, sizeof(tmpstr), &qinfo) < 0)
		return -1;
	(void)write(s, tmpstr, strlen(tmpstr)+1);

	return 0;
}

/*
 * Derived from the rcmd() libc call, with modified interface.
 * This version is MT-safe.  Errors are displayed in pdsh-compat format.
 * Connection can time out.
 *	ahost (IN)		target hostname
 *	locuser (IN)		local username
 *	remuser (IN)		remote username
 *	cmd (IN)		remote command to execute under shell
 *	nodeid (IN)		node index for this connection
 *	fd2p (IN)		if non NULL, return stderr file descriptor here
 *	int (RETURN)		-1 on error, socket for I/O on success
 */
int 
qcmd(char *ahost, char *addr, char *locuser, char *remuser, char *cmd, 
		int nodeid, int *fd2p)
{
	struct sockaddr_in sin, from;
	fd_set reads;
	sigset_t oldset, blockme;
	pid_t pid;
	int s, lport, timo, rv, maxfd;
	char c;

	pid = getpid();
	sigemptyset(&blockme);
	sigaddset(&blockme, SIGURG);
	pthread_sigmask(SIG_BLOCK, &blockme, &oldset);
	for (timo = 1, lport = IPPORT_RESERVED - 1;;) {
		s = rresvport(&lport);
		if (s < 0) {
			if (errno == EAGAIN)
				err("%p: %S: rcmd: socket: all ports in use\n",
				    ahost);
			else
				err("%p: %S: rcmd: socket: %m\n", ahost);
			pthread_sigmask(SIG_SETMASK, &oldset, NULL);
			return (-1);
		}
		fcntl(s, F_SETOWN, pid);
		sin.sin_family = AF_INET;
		memcpy(&sin.sin_addr, addr, IP_ADDR_LEN);
		sin.sin_port = htons(QSHELL_PORT);
		rv = connect(s, (struct sockaddr *)&sin, sizeof(sin));
		if (rv >= 0)
			break;
		(void)close(s);
		if (errno == EADDRINUSE) {
			lport--;
			continue;
		}
		if (errno == ECONNREFUSED && timo <= 16) {
			(void)sleep(timo);
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
		socklen_t len = sizeof(from); /* arg to accept */

		if (s2 < 0)
			goto bad;
		listen(s2, 1);
		(void)sprintf(num, "%d", lport);
		if (write(s, num, strlen(num)+1) != strlen(num)+1) {
			err("%p: %S: rcmd: write (setting up stderr): %m\n", 
			    ahost);
			(void)close(s2);
			goto bad;
		}
		FD_ZERO(&reads);
		FD_SET(s, &reads);
		FD_SET(s2, &reads);
		errno = 0;
		maxfd = (s > s2) ? s : s2;
		if (select(maxfd + 1, &reads, 0, 0, 0) < 1 
		    || !FD_ISSET(s2, &reads)) {
			if (errno != 0)
				err("%p: %S: rcmd: select (setting up stderr): %m\n", ahost);
			else
				err("%p: %S: select: protocol failure in circuit setup\n", ahost);
			(void)close(s2);
			goto bad;
		}
		s3 = accept(s2, (struct sockaddr *)&from, &len);
		(void)close(s2);
		if (s3 < 0) {
			err("%p: %S: rcmd: accept: %m\n", ahost);
			lport = 0;
			goto bad;
		}
		*fd2p = s3;
		from.sin_port = ntohs((u_short)from.sin_port);
		if (from.sin_family != AF_INET ||
		    from.sin_port >= IPPORT_RESERVED ||
		    from.sin_port < IPPORT_RESERVED / 2) {
			err("%p: %S: socket: protocol failure in circuit setup\n", 
			    ahost);
			goto bad2;
		}
	}
	(void)write(s, locuser, strlen(locuser)+1);
	(void)write(s, remuser, strlen(remuser)+1);
	(void)write(s, cmd, strlen(cmd)+1);
	if (qcmd_send_extra_args(s, nodeid) < 0)
		goto bad2;

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
		(void)close(*fd2p);
bad:
	(void)close(s);
	pthread_sigmask(SIG_SETMASK, &oldset, NULL);
	return (-1);
}
#endif /* HAVE_ELAN */

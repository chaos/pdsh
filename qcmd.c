/* 
 * $Id$  
 */

/*
 * Started with BSD rcmd.c, modified thus:
 *
 * Sends additional arguments to remote:
 * - current working directory
 * - count of environment vars (client plus some expected by Elan MPICH)
 * - environment vars themselves
 * - Elan capability 128-bit user key, randomly generated
 * - Elan hw program number, randomly generated
 * - Elan hw context number, randomly generated
 * - List of Elan ID's for nodes participating in job (for setting up Elan
 *   capability)
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

#include <conf.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <pthread.h>

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

#include <dsh.h>
#include <err.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "conf.h"
#include "xstring.h"
#include "list.h"
#if HAVE_ELAN
#include <elan3/elan3.h>
#include <elan3/elanvp.h>
#endif

#ifndef ELAN_USER_BASE_CONTEXT_NUM
#define ELAN_USER_BASE_CONTEXT_NUM 0x20
#endif
#ifndef ELAN_USER_TOP_CONTEXT_NUM
#define ELAN_USER_TOP_CONTEXT_NUM 0x7ff
#endif

#define ELAN_PRG_START	0
#define ELAN_PRG_END	1024

#define QSHELL_PORT 523

#if HAVE_GETHOSTBYNAME_R
#define HBUF_LEN	1024
#endif


extern char **environ;

static char cwd[MAXPATHLEN+1];
static uint32_t key[4];
static uint32_t prgnum;
static uint32_t ctxt;
static char *nodelist;
static int nnodes;


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
                c = (char)signum;
                write(efd, &c, 1);
        }
}

/* 
 * Convert hostname to elan node number.  This version just returns
 * the numerical part of the hostname.  
 * XXX When RMS is in use, this is a valid assumption; in future, use genders 
 * to determine host->elanId mapping.
 * 	host (IN)		hostname
 *	nodenum (RETURN)	numerical portion of hostname
 */
static char *
elanid(char *host)
{
	char *nodenum = host;

	while (*nodenum && !isdigit(*nodenum))
		nodenum++;
	return nodenum;
}

/* 
 * This is called once before all the parallel qcmd invocations to
 * initialize globals containing current working directory, 128-bit Elan
 * capability userkey, Elan program number, Elan context, list of Elan
 * node ID's, and count of target nodes. 
 * 	wcoll (IN)	list of nodes
 */
void
qcmd_init(list_t wcoll)
{
	int i;
	list_t tmplist = list_new();

	/* record current working directory */
	if (getcwd(cwd, sizeof(cwd)) == NULL)
		errx("%p: getcwd failed\n");

	/* generate 128-bit key for elan context */
	srand48(getpid());
	for (i = 0; i < sizeof(key) / sizeof(key[0]); i++) {
		key[i] = lrand48();
	}

	/* generate random elan program number */
	prgnum = lrand48() % (ELAN_PRG_END - ELAN_PRG_START);	
	prgnum += ELAN_PRG_START;

	/* generate random elan context number */
	ctxt = lrand48() % 
		(ELAN_USER_TOP_CONTEXT_NUM - ELAN_USER_BASE_CONTEXT_NUM + 1);
	ctxt += ELAN_USER_BASE_CONTEXT_NUM;

	/* build string containing comma-sep elan node id's from wcoll */
	for (i = 0; i < list_length(wcoll); i++) {
		list_push(tmplist, elanid(list_nth(wcoll, i)));
	}
	nodelist = list_join(",", tmplist);
	list_free(&tmplist);

	/* count of nodes in working collective */
	nnodes = list_length(wcoll);
}

/*
 * Write to the remote xrshd the number of environment vars to follow, 
 * then the variables themselves.  The environment consists of pdsh's 
 * environment plus RMS variables.
 *	s (IN)		socket 
 *	rank (IN)	MPI rank for this connection
 */
static void
env_write(int s, int rank)
{
	char **ep;
	char tmpstr[16];
	int count = 0;

	/* count them and add 5 for RMS_* variables */
	for (ep = environ; *ep != NULL; ep++)
		count++;
	sprintf(tmpstr, "%d", count + 5);
	(void)write(s, tmpstr, strlen(tmpstr)+1);

	/* write them */
	for (ep = environ; *ep != NULL; ep++)
		(void)write(s, *ep, strlen(*ep)+1);

	sprintf(tmpstr, "RMS_RANK=%d", rank);
	(void)write(s, tmpstr, strlen(tmpstr)+1);

	sprintf(tmpstr, "RMS_NODEID=%d", rank);
	(void)write(s, tmpstr, strlen(tmpstr)+1);

	sprintf(tmpstr, "RMS_PROCID=%d", rank);
	(void)write(s, tmpstr, strlen(tmpstr)+1);

	sprintf(tmpstr, "RMS_NNODES=%d", nnodes);
	(void)write(s, tmpstr, strlen(tmpstr)+1);

	sprintf(tmpstr, "RMS_NPROCS=%d", nnodes);
	(void)write(s, tmpstr, strlen(tmpstr)+1);
}

/*
 * Write to the remote xrshd the Elan capability user key, hardware context
 * number, program number, and list of Elan node ID's.
 *	s (IN)		socket 
 */
static void
elan_write(int s)
{
	char tmpstr[128];

	/* send capability userkey */
       	sprintf(tmpstr, "%x.%x.%x.%x", key[0], key[1], key[2], key[3]);
	(void)write(s, tmpstr, strlen(tmpstr)+1);

	/* send "program description" */
	sprintf(tmpstr, "%x", prgnum);
	(void)write(s, tmpstr, strlen(tmpstr)+1);

	/* XXX need to deal with hi.lo.my values when tasks/cpu > 1 */
	sprintf(tmpstr, "%x.%x.%x", ctxt, ctxt, ctxt);
	(void)write(s, tmpstr, strlen(tmpstr)+1);
							               
	/* write list of nodes */
	(void)write(s, nodelist, strlen(nodelist)+1);
}

/*
 * Derived from the rcmd() libc call, with modified interface.
 * This version is MT-safe.  Errors are displayed in pdsh-compat format.
 * Connection can time out.
 *	ahost (IN)		target hostname
 *	locuser (IN)		local username
 *	remuser (IN)		remote username
 *	cmd (IN)		remote command to execute under shell
 *	rank (IN)		MPI rank for this connection
 *	fd2p (IN)		if non NULL, return stderr file descriptor here
 *	int (RETURN)		-1 on error, socket for I/O on success
 */
int 
qcmd(char *ahost, char *locuser, char *remuser, char *cmd, int rank, int *fd2p)
{
	struct hostent *hp;
	struct sockaddr_in sin, from;
	fd_set reads;
	sigset_t oldset, blockme;
	pid_t pid;
	int s, lport, timo, rv, maxfd;
	char c;
#if HAVE_GETHOSTBYNAME_R
	struct hostent he;
	char hbuf[HBUF_LEN];
	int h_errnox;
	static pthread_mutex_t mylock = PTHREAD_MUTEX_INITIALIZER;
#endif
	pid = getpid();
#if HAVE_GETHOSTBYNAME_R
	memset(hbuf, 0, HBUF_LEN);
#ifdef __linux
	pthread_mutex_lock(&mylock); 	/* RH 6.2 - race on /etc/hosts.conf? */
	/* linux compat args */
	(void)gethostbyname_r(ahost, &he, hbuf, HBUF_LEN, &hp, &h_errnox);
	pthread_mutex_unlock(&mylock);
#else	/* solaris compat args */
	hp = gethostbyname_r(ahost, &he, hbuf, HBUF_LEN, &h_errnox);
#endif
#else
	hp = gethostbyname(ahost);	/* otherwise assumed MT-safe */
					/* true on AIX4.3, OSF1 V4.0 */
#endif
	if (hp == NULL) {
		err("%p: gethostbyname: lookup of %S failed\n", ahost);
		return (-1);
	}

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
		sin.sin_family = hp->h_addrtype;
		memcpy(&sin.sin_addr, hp->h_addr_list[0], hp->h_length);
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
		if (hp->h_addr_list[1] != NULL) {
			err("%p: %S: connect to address %s: %m\n", 
			    ahost, inet_ntoa(sin.sin_addr));
			hp->h_addr_list++;
			memcpy(&sin.sin_addr, hp->h_addr_list[0], hp->h_length);
			err("%p: %S: trying %s...\n", 
			    ahost, inet_ntoa(sin.sin_addr));
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
#ifdef _AIX
		unsigned long len = sizeof(from); /* arg to accept */
#else
		int len = sizeof(from); /* arg to accept */
#endif

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
	(void)write(s, cwd, strlen(cwd)+1);
	env_write(s, rank);
	elan_write(s);

	if (read(s, &c, 1) != 1) {
		err("%p: %S: read: %m\n", ahost);
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


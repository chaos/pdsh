/* 
 * $Id$ 
 */

/*
 * This is the MIT Kerberos IV kcmd.c with some athena hacks removed and 
 * MT safety added, and the interface changed.
 *
 * XXX for some reason it takes a really long time (several seconds per node)
 * to acquire service tickets!  Why?
 *
 * Call k4cmd_init() before parallel calls to k4cmd().
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

#include "conf.h"

#include <pthread.h>

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
#include <sys/select.h>
#include <fcntl.h>	/* for F_SETOWN */

#include "xmalloc.h"
#include "xstring.h"
#include "dsh.h"
#include "err.h"
#include "list.h"

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 64
#endif

#define KCMD_PORT 544

extern		errno;
extern char     *inet_ntoa();

void 
k4cmd_init(list_t wcoll)
{
	/* not implemented */
}

/*
 * Use rcmd backchannel to propagate signals.
 *      efd (IN)        file descriptor connected socket (-1 if not used)
 *      signum (IN)     signal number to send
 */
void
k4cmd_signal(int efd, int signum)
{
        char c;

        if (efd >= 0) {
                c = (char)signum;
                write(efd, &c, 1);
        }
}

/*
 * The rcmd call itself.
 *      ahost (IN)      remote hostname
 *      locuser (IN)    local username
 *      remuser (IN)    remote username
 *      cmd (IN)        command to execute
 *       rank (IN)	MPI rank
 *      fd2p (IN/OUT)   if non-NULL, open stderr backchannel on this fd
 *      s (RETURN)      socket for stdout/sdin or -1 on failure
 */
int 
k4cmd(char *ahost, char *locuser, char *remuser, char *cmd, int rank, int *fd2p)
{
        KTEXT_ST ticket;        /* kerberos IV context */
        CREDENTIALS cred;
        Key_schedule schedule;
        MSG_DAT msg_data;
        struct sockaddr_in faddr;
        struct sockaddr_in laddr;

	int             s, pid;
	sigset_t oldset, blockme;
	struct sockaddr_in sin, from;
	char            c;
	int             lport = IPPORT_RESERVED - 1;
	char		*host_save;
	unsigned long	krb_options = 0L;
	static pthread_mutex_t mylock = PTHREAD_MUTEX_INITIALIZER;
#if HAVE_GETHOSTBYNAME_R
        char hbuf[HBUF_LEN];
        int h_errnox;
        struct hostent he;
#else
#endif
	struct hostent *hp;
	int		status;
	int             rc, rv;
	fd_set          reads;
	int 		maxfd;
	char		*tmpstr;

	pid = getpid();
#if HAVE_GETHOSTBYNAME_R
#ifdef __linux
	pthread_mutex_lock(&mylock); 	/* RH 6.2 - race on /etc/hosts.conf */
	/* linux compat args */
	(void)gethostbyname_r(ahost, &he, hbuf, HBUF_LEN, &hp, &h_errnox);
	pthread_mutex_unlock(&mylock);
#else
	/* solaris compat args */
        hp = gethostbyname_r(ahost, &he, hbuf, HBUF_LEN, &h_errnox);
#endif
#else
        hp = gethostbyname(ahost);	/* assumed MT-safe */
					/* true on AIX4.3, OSF1 V4.0 */
#endif
	if (hp == 0) {
		err("%p: gethostbyname: lookup of %S failed\n", ahost);
		return (-1);
	}

	sigemptyset(&blockme);
	sigaddset(&blockme, SIGURG);
	pthread_sigmask(SIG_BLOCK, &blockme, &oldset);
	for (;;) {
		s = rresvport(&lport);
		if (s < 0) {
			if (errno == EAGAIN)
				err("%p: %S: socket: All ports in use\n",ahost);
			else
				err("%p: %S: k4cmd: socket: %m\n", ahost);
			pthread_sigmask(SIG_SETMASK, &oldset, NULL);
			return (-1);
		}
		fcntl(s, F_SETOWN, pid);
		sin.sin_family = hp->h_addrtype;
		memcpy((caddr_t) & sin.sin_addr, hp->h_addr_list[0], hp->h_length);
		sin.sin_port = htons(KCMD_PORT);

                rv = connect(s, (struct sockaddr *)&sin, sizeof(sin));

                if (rv >= 0)
                        break;
		(void) close(s);
		if (errno == EADDRINUSE) {
			lport--;
			continue;
		}
		if (hp->h_addr_list[1] != NULL) {
			err("%p: %S: connect to address %s: %m", 
			    ahost, inet_ntoa(sin.sin_addr));
			hp->h_addr_list++;
			memcpy((caddr_t) & sin.sin_addr, hp->h_addr_list[0],
			       hp->h_length);
			err("%p: %S: trying %s...\n", 
			    ahost, inet_ntoa(sin.sin_addr));
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
		char            num[8];
		int             s2, s3;
		unsigned long 	len = sizeof(from);

		s2 = rresvport(&lport);
		if (s2 < 0) {
			goto bad;
		}
		listen(s2, 1);
                (void)sprintf(num, "%d", lport);
		if (write(s, num, strlen(num) + 1) != strlen(num) + 1) {
			err("%p: %S: write: setting up stderr: %m\n", ahost);
			(void) close(s2);
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
				err("%p: %S: k4cmd: select (setting up stderr): %m\n", ahost);
			else
				err("%p: %S: select: protocol failure in circuit setup\n", ahost);
			(void) close(s2);
			goto bad;
		}
		s3 = accept(s2, (struct sockaddr *) & from, &len);
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
	/*krb_options |= KOPT_DONT_CANON;*/
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
			tmpbuf[3] = NULL;
			err("%p: %S: %s", ahost, tmpbuf);
			*old_data = (-1);
		}
		if ((status == KFAILURE) && (*old_data == (char) -1)) {
			while (read(s, &c, 1) == 1) {
				/*(void) write(2, &c, 1);*/
				*p++ = c;
				if (c == '\n')
					break;
			}
			*p++ = NULL;
			err("%p: %S: %s", ahost, tmpbuf);
			status = -1;
		}
		switch (status) {
                        case KDC_PR_UNKNOWN:
                                err("%p: %S: not registered for kerberos\n", 
				    ahost);
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

reread:
	if ((rc = read(s, &c, 1)) != 1) {
		if (rc == -1) {
			err("%p: %S: read: %m\n", ahost);
		} else {
			err("%p: %S: k4cmd: bad connection with remote host\n", 
			    ahost);
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
                *p++ = NULL;
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

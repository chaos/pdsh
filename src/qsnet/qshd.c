/*-
 * Copyright (c) 1988, 1989 The Regents of the University of California.
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

/*
 * PAM modifications by Michael K. Johnson <johnsonm@redhat.com>
 */

char copyright[] =
 "@(#) Copyright (c) 1988, 1989 The Regents of the University of California.\n"
 "All rights reserved.\n";

/*
 * From: @(#)rshd.c	5.38 (Berkeley) 3/2/91
 */
char rcsid[] = 
  "$Id$";
/*#include "../version.h"*/

/*
 * remote shell server:
 *	[port]\0
 *	remuser\0
 *	locuser\0
 *	command\0
 *	current working directory\0
 *	environment var count\0
 *	environment var 1\0
 *	environment var 2\0
 *	...
 *	environment var (count)\0
 *	elan userkey\0
 *	elan prgnum\0
 *	elan ctxt\0
 *	elan nodelist\0
 *	data
 */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <pwd.h>
#include <grp.h>
#include <syslog.h>
#include <resolv.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>  /* for vsnprintf */
#include <stdlib.h>
#include <string.h>
#include <paths.h>
#include <stdarg.h>
#include <ctype.h>
#include <assert.h>

#include "conf.h"

#if	HAVE_ELAN3
#include <elan3/elan3.h>
#include <elan3/elanvp.h>
#include <rms/rmscall.h>
#endif

#if defined(__GLIBC__) && (__GLIBC__ >= 2)
#define _check_rhosts_file  __check_rhosts_file
#endif

#ifdef USE_PAM
#include <security/pam_appl.h>
#include <security/pam_misc.h>
static pam_handle_t *pamh;
#endif /* USE_PAM */

#define	OPTIONS	"ahlLn"

static int keepalive = 1;
static int check_all = 0;
static int paranoid = 0;
static int sent_null;
static int allow_root_rhosts=1;

char	username[20] = "USER=";
char	homedir[64] = "HOME=";
char	shell[64] = "SHELL=";
char	path[100] = "PATH=";
char	*envinit[] =
	    {homedir, shell, path, username, 0};
extern	char	**environ;

static void error(const char *fmt, ...);
static void doit(struct sockaddr_in *fromp);
static void getstr(char *buf, int cnt, const char *err);

extern int _check_rhosts_file;

/*
 * Report error to client.
 * Note: can't be used until second socket has connected
 * to client, or older clients will hang waiting
 * for that connection first.
 */
static void
error(const char *fmt, ...) {
    va_list ap;
    char buf[BUFSIZ], *bp = buf;
    
    if (sent_null == 0)	*bp++ = 1;
    va_start(ap, fmt);
    vsnprintf(bp, sizeof(buf)-1, fmt, ap);
    va_end(ap);
    write(2, buf, strlen(buf));
}

/* same as above but also syslog the error (jg) */
static void
errlog(const char *fmt, ...) {
    va_list ap;
    char buf[BUFSIZ], *bp = buf;
    
    if (sent_null == 0)	*bp++ = 1;
    va_start(ap, fmt);
    vsnprintf(bp, sizeof(buf)-1, fmt, ap);
    va_end(ap);
    syslog(LOG_ERR, (sent_null == 0) ? buf + 1 : buf);
    write(2, buf, strlen(buf));
}

static void fail(const char *errorstr, 
		 const char *remuser, const char *hostname, 
		 const char *locuser,
		 const char *cmdbuf) 
{
	/* log the (failed) rsh request */
	syslog(LOG_INFO|LOG_AUTH, "rsh denied to %s@%s as %s: %s",
	       remuser, hostname, locuser, errorstr);
	if (paranoid) {
	    syslog(LOG_INFO|LOG_AUTH, "rsh command was '%s'", cmdbuf);
	}
	error(errorstr, hostname);
	exit(1);
}

static void getstr(char *buf, int cnt, const char *err) {
    char c;
    do {
	if (read(0, &c, 1) != 1) exit(1);
	*buf++ = c;
	if (--cnt == 0) {
	    error("%s too long\n", err);
	    exit(1);
	}
    } while (c != 0);
}

static int getint(void) {
    int port = 0;
    char c;
    do {
	if (read(0, &c, 1) != 1) exit(1);
	if (isascii(c) && isdigit(c)) port = port*10 + c-'0';
    } while (c != 0);
    return port;
}

static void stderr_parent(int sock, int pype, int pid) {
    fd_set ready, readfrom;
    char buf[BUFSIZ], sig;
    int one = 1;
    int nfd, cc, guys=2;
    
    ioctl(pype, FIONBIO, (char *)&one);
    /* should set s nbio! */
    
    FD_ZERO(&readfrom);
    FD_SET(sock, &readfrom);
    FD_SET(pype, &readfrom);
    if (pype > sock) nfd = pype+1;
    else nfd = sock+1;
    
    while (guys > 0) {
	ready = readfrom;
	if (select(nfd, &ready, NULL, NULL, NULL) < 0) {
	   if (errno != EINTR) {
	      break;
	   }
	   continue;
	}
	if (FD_ISSET(sock, &ready)) {
	    cc = read(sock, &sig, 1);
	    if (cc <= 0) {
	       FD_CLR(sock, &readfrom);
	       guys--;
	    }
#if	HAVE_ELAN
	    /* pid is the "program description" if using Elan */
	    else rms_prgsignal(pid, sig);
#else
	    else killpg(pid, sig);
#endif
	}
	if (FD_ISSET(pype, &ready)) {
	    cc = read(pype, buf, sizeof(buf));
	    if (cc <= 0) {
		shutdown(sock, 2);
		FD_CLR(pype, &readfrom);
		guys--;
	    } 
	    else write(sock, buf, cc);
	}
    }
    
#ifdef USE_PAM
    /*
     * This does not strike me as the right place for this; this is
     * in a child process... what does this need to accomplish?
     *
     * No, it's not the child process, the code is just confusing.
     */
    pam_close_session(pamh, 0);
    pam_end(pamh, PAM_SUCCESS);
#endif
    exit(0);
}


static struct passwd *doauth(const char *remuser, 
			     const char *hostname, 
			     const char *locuser)
{
#ifdef USE_PAM
    static struct pam_conv conv = { misc_conv, NULL };
    int retcode;
#endif
    struct passwd *pwd = getpwnam(locuser);
    if (pwd == NULL) return NULL;
    if (pwd->pw_uid==0) paranoid = 1;

#ifdef USE_PAM
    retcode = pam_start("rsh", locuser, &conv, &pamh);
    if (retcode != PAM_SUCCESS) {
	syslog(LOG_ERR, "pam_start: %s\n", pam_strerror(pamh, retcode));
	exit (1);
    }
    pam_set_item (pamh, PAM_RUSER, remuser);
    pam_set_item (pamh, PAM_RHOST, hostname);
    pam_set_item (pamh, PAM_TTY, "tty");
    
    retcode = pam_authenticate(pamh, 0);
    if (retcode == PAM_SUCCESS) {
	retcode = pam_acct_mgmt(pamh, 0);
    }
    if (retcode == PAM_SUCCESS) {
	/*
	 * Why do we need to set groups here?
	 * Also, this stuff should be moved down near where the setuid() is.
	 */
	if (setgid(pwd->pw_gid) != 0) {
	    pam_end(pamh, PAM_SYSTEM_ERR);
	    return NULL;
	}
	if (initgroups(locuser, pwd->pw_gid) != 0) {
	    pam_end(pamh, PAM_SYSTEM_ERR);
	    return NULL;
	}
	retcode = pam_setcred(pamh, PAM_ESTABLISH_CRED);
    }
    
    if (retcode == PAM_SUCCESS) {
	retcode = pam_open_session(pamh,0);
    }
    if (retcode != PAM_SUCCESS) {
	pam_end(pamh, retcode);
	return NULL;
    }
    return pwd;
#else
    if (pwd->pw_uid==0 && !allow_root_rhosts) return NULL;
    if (ruserok(hostname, pwd->pw_uid==0, remuser, locuser) < 0) {
	return NULL;
    }
    return pwd;
#endif
}

static const char *findhostname(struct sockaddr_in *fromp,
				const char *remuser, const char *locuser,
				const char *cmdbuf) 
{
	struct hostent *hp;
	const char *hostname;

	hp = gethostbyaddr((char *)&fromp->sin_addr, sizeof (struct in_addr),
			   fromp->sin_family);

	errno = ENOMEM; /* malloc (thus strdup) may not set it */
	if (hp) hostname = strdup(hp->h_name);
	else hostname = strdup(inet_ntoa(fromp->sin_addr));

	if (hostname==NULL) {
	    /* out of memory? */
	    error("strdup: %s\n", strerror(errno));
	    exit(1);
	}

	/*
	 * Attempt to confirm the DNS. 
	 */
#ifdef	RES_DNSRCH
	_res.options &= ~RES_DNSRCH;
#endif
	hp = gethostbyname(hostname);
	if (hp == NULL) {
	    syslog(LOG_INFO, "Couldn't look up address for %s", hostname);
	    fail("Couldn't get address for your host (%s)\n", 
		 remuser, inet_ntoa(fromp->sin_addr), locuser, cmdbuf);
	} 
	while (hp->h_addr_list[0] != NULL) {
	    if (!memcmp(hp->h_addr_list[0], &fromp->sin_addr,
			sizeof(fromp->sin_addr))) {
		return hostname;
	    }
	    hp->h_addr_list++;
	}
	syslog(LOG_NOTICE, "Host addr %s not listed for host %s",
	       inet_ntoa(fromp->sin_addr), hp->h_name);
	fail("Host address mismatch for %s\n", 
	     remuser, inet_ntoa(fromp->sin_addr), locuser, cmdbuf);
	return NULL; /* not reachable */
}

#if	HAVE_ELAN3
/*
 * Turn a comma separated list of nodes into cap->LowNode, cap->HighNode,
 * cap->Entries, and cap->Bitmap.
 * 	cap (IN/OUT)	Elan capability structure
 * 	instring (IN)	comma separated list of node numbers
 * Returns 0 on success, -1 on failure.
 */
static int
elan_nodelist(ELAN_CAPABILITY *cap, char *instring)
{
	char *scpy, *tok, *s;
	int node;

	/* determine high and low node numbers */
	cap->HighNode = cap->LowNode = -1;
	s = scpy = strdup(instring);
	while ((tok = strtok(s, ",")) != NULL) {
		node = atoi(tok);
		if (node < cap->LowNode || cap->LowNode == -1)
			cap->LowNode = node;
		if (node > cap->HighNode || cap->HighNode == -1)
			cap->HighNode = node;
		s = NULL;
	}
	free(scpy);
	if (cap->HighNode == -1 || cap->LowNode == -1)
		return -1;
	/*if (cap->HighNode < cap->LowNode)
		return -1;*/

	/* set bits corresponding to node#'s, normalized at LowNode = bit 0 */
	cap->Entries = 0;
	s = scpy = strdup(instring);
	while ((tok = strtok(s, ",")) != NULL) {
		node = atoi(tok);
		BT_SET(cap->Bitmap, node - cap->LowNode);
		s = NULL;
		cap->Entries++;
	}
	free(scpy);
	if (cap->Entries == 0)
		return -1;

	return 0;
}
#endif

static void
doit(struct sockaddr_in *fromp)
{
	char cmdbuf[ARG_MAX+1];
	const char *theshell, *shellname;
	char locuser[16], remuser[16];
	struct passwd *pwd;
	int sock = -1;
	const char *hostname;
	u_short port;
	int pv[2], pid, ifd;
	char cwd[MAXPATHLEN+1];
	char envstr[1024];
	char tmpstr[1024];
	int envcount;
	int prgnum;
#if	HAVE_ELAN3
	ELAN_CAPABILITY cap;
	ELAN3_CTX *ctx;
#endif
	signal(SIGINT, SIG_DFL);
	signal(SIGQUIT, SIG_DFL);
	signal(SIGTERM, SIG_DFL);

	/*
	 * Remote side sends us a stringified port number to send back
	 * stderr on.  If zero, stderr is folded in with stdout.
	 */
	alarm(60);
	port = getint();
	alarm(0);

	if (port != 0) {
		int lport = IPPORT_RESERVED - 1;
		sock = rresvport(&lport);
		if (sock < 0) {
		    syslog(LOG_ERR, "can't get stderr port: %m");
		    exit(1);
		}
		if (port >= IPPORT_RESERVED) {
		    syslog(LOG_ERR, "2nd port not reserved\n");
		    exit(1);
		}
		fromp->sin_port = htons(port);
		if (connect(sock, (struct sockaddr *)fromp,
			    sizeof(*fromp)) < 0) {
		    syslog(LOG_INFO, "connect second port: %m");
		    exit(1);
		}
	}

#if 0
	/* We're running from inetd; socket is already on 0, 1, 2 */
	dup2(f, 0);
	dup2(f, 1);
	dup2(f, 2);
#endif
	/* 
	 * Second, third, and fourth args are remote and local user, and the 
	 * command to execute under the shell.
	 */
	getstr(remuser, sizeof(remuser), "remuser");
	getstr(locuser, sizeof(locuser), "locuser");
	getstr(cmdbuf, sizeof(cmdbuf), "command");

	/*
	 * Begin arguments added for qshell.
	 */

	/* read cwd of client - will change to cwd on remote side */
	getstr(cwd, sizeof(cwd), "cwd");

	/* read environment of client - will replicate on remote side */
	getstr(tmpstr, sizeof(tmpstr), "envcount");
	envcount = atoi(tmpstr);
	while (envcount-- > 0) {
		getstr(envstr, sizeof(envstr), "envstr");
		putenv(strdup(envstr));
	}


	/* init elan capability and read the 128-bit user key */
	getstr(tmpstr, sizeof(tmpstr), "userkey");
#if	HAVE_ELAN3
	elan3_nullcap(&cap);
	cap.Type = ELAN_CAP_TYPE_BLOCK;
	cap.Type |= ELAN_CAP_TYPE_BROADCASTABLE;
	cap.Type |= ELAN_CAP_TYPE_MULTI_RAIL;
	cap.RailMask = 1;
	if (sscanf(tmpstr, "%x.%x.%x.%x", &cap.UserKey.Values[0],
			&cap.UserKey.Values[1], &cap.UserKey.Values[2],
			&cap.UserKey.Values[3]) != 4) {
		errlog("error reading userkey: %s", tmpstr);
		exit(1);
	}
#endif
	/* read the "program description" (similar to process group) */
	getstr(tmpstr, sizeof(tmpstr), "prgnum");
	if (sscanf(tmpstr, "%x", &prgnum) != 1) {
		errlog("error reading prgnum: %s", tmpstr);
		exit(1);
	}

	/* read elan hw context numbers */
	getstr(tmpstr, sizeof(tmpstr), "context");
#if	HAVE_ELAN3
	if (sscanf(tmpstr, "%x.%x.%x", &cap.LowContext, &cap.HighContext, 
				&cap.MyContext) != 3) {
		errlog("error reading context: %s", tmpstr);
		exit(1);
	}
#endif
	/* read list of node numbers for capability bitmap, low/hi node */
	getstr(tmpstr, sizeof(tmpstr), "nodelist");
#if	HAVE_ELAN3
	if (elan_nodelist(&cap, tmpstr) < 0) {
		errlog("error reading nodelist: %s", tmpstr);
		exit(1);
	}
#endif
	/* 
	 * End qshell arguments.
	 */

	/* 
	 * A single \0 written back on the socket indicates success.  Error 
	 * would be inidicated by a nonzero code followed by error string,
	 * e.g. through calling error() or fail() above.
	 */
	(void) write(2, "\0", 1);
	sent_null = 1;

	if (!strcmp(locuser, "root")) paranoid = 1;

	hostname = findhostname(fromp, remuser, locuser, cmdbuf);

	setpwent();
	pwd = doauth(remuser, hostname, locuser);
	if (pwd == NULL) {
		fail("Permission denied.\n", 
		     remuser, hostname, locuser, cmdbuf);
	}
	if (pwd->pw_uid != 0 && !access(_PATH_NOLOGIN, F_OK)) {
		error("Logins currently disabled.\n");
		exit(1);
	}

	/* 
	 * Fork here.  Parent waits for child to terminate, then cleans up.
	 */
	pid = fork();
	switch (pid) {
		case -1:	/* error */
			errlog("fork: %s", strerror(errno));
			exit(1);
		case 0:		/* child falls thru */
			break;
		default:	/* parent */
			if (waitpid(pid, NULL, 0) < 0) {
				errlog("waitpid: %s", strerror(errno));
				exit(1);
			}
			syslog(LOG_DEBUG, "cleaning up prg %d", prgnum);
			rms_prgsignal(prgnum, SIGKILL);
			if (rms_prgdestroy(prgnum) < 0) {
				errlog("rms_prgdestroy: %s", strerror(errno));
			}
			exit(0);
	}
	/* child continues here */

#if	HAVE_ELAN3
	/*
	 * At this point we are authenticated to run as 'locuser' but are 
	 * still root.
	 */
	/* obtain an Elan context to use in call to elan3_create */
	if ((ctx = _elan3_init(0)) == NULL) {
		errlog("_elan3_init failed: %s", strerror(errno));
		exit(1);
	}
	/* associate this process and its children with prgnum */
	if (rms_prgcreate(prgnum, pwd->pw_uid, 2) < 0) { /* 2 cpus */
		errlog("rms_prgcreate failed: %s", strerror(errno));
		exit(1);
	}
	/* make cap known via rms_getcap/rms_ncaps to members of this prgnum */
	if (elan3_create(ctx, &cap) < 0) {
		errlog("elan3_create failed: %s", strerror(errno));
		exit(1);
	}
	if (rms_prgaddcap(prgnum, 0, &cap) < 0) {
		errlog("rms_prgaddcap failed: %s", strerror(errno));
		exit(1);
	}
	syslog(LOG_DEBUG, "prg %d cap %s bitmap 0x%.8x", prgnum,
			elan3_capability_string(&cap, tmpstr), cap.Bitmap[0]);
#endif

	/*
 	 * Fork off a process to send back stderr if requested.
	 */
	if (port) {
		if (pipe(pv) < 0) {
			error("Can't make pipe.\n");
			exit(1);
		}
		pid = fork();
		if (pid == -1)  {
			error("Can't fork; try again.\n");
			exit(1);
		}
		if (pid) {
			close(0); 
			close(1);
			close(2); 
			close(pv[1]);
#if	HAVE_ELAN
			stderr_parent(sock, pv[0], prgnum);
#else
			stderr_parent(sock, pv[0], pid);
#endif
			/* NOTREACHED */
		}
		setpgrp();
		close(sock); 
		close(pv[0]);
		dup2(pv[1], 2);
		close(pv[1]);
	}


#if	HAVE_ELAN3
	/*
	 * Assign elan hardware context to current process.
	 * This is a context index, not the context number.
	 */
	if (rms_setcap(0, 0) < 0) {
		errlog("rms_setcap: %s", strerror(errno));
		exit(1);
	}
#endif
	/* 
	 * Fork again.  It seems that setcap needs to happen in the parent
	 * of the user process.
	 */
	pid = fork();
	switch (pid) {
		case -1:	/* error */
			errlog("fork: %s", strerror(errno));
			exit(1);
		case 0:	 	/* child falls through */
			break;
		default:	/* parent...just need to wait */
			if (waitpid(pid, NULL, 0) < 0) {
				errlog("waitpid: %s", strerror(errno));
				exit(1);
			}
			exit(0);
	}
	/* child falls through here */

	/*
	 *  Become the locuser, etc. etc. then exec the shell command.
	 */

	/* set the path to the shell */
	theshell = pwd->pw_shell;
	if (!theshell || !*theshell) {
	    /* shouldn't we deny access? */
	    theshell = _PATH_BSHELL;
	}
#if BSD > 43
	if (setlogin(pwd->pw_name) < 0) {
		errlog("setlogin() failed: %s", 
				strerror(errno));
	}
#endif
#ifndef USE_PAM
	/* if PAM, already done */
	if (setgid(pwd->pw_gid)) {
		errlog("setgid: %s", strerror(errno));
		exit(1);
	}
	if (initgroups(pwd->pw_name, pwd->pw_gid)) {
		errlog("initgroups: %s", strerror(errno));
		exit(1);
	}
#endif
	if (setuid(pwd->pw_uid)) {
		errlog("setuid: %s", strerror(errno));
		exit(1);
	}

	strncat(homedir, pwd->pw_dir, sizeof(homedir)-6);
	homedir[sizeof(homedir)-1] = 0;

	strcat(path, _PATH_DEFPATH);

	strncat(shell, theshell, sizeof(shell)-7);
	shell[sizeof(shell)-1] = 0;

	strncat(username, pwd->pw_name, sizeof(username)-6);
	username[sizeof(username)-1] = 0;

	shellname = strrchr(theshell, '/');
	if (shellname) 
		shellname++;
	else 
		shellname = theshell;

	endpwent();

	if (paranoid) {
		syslog(LOG_INFO|LOG_AUTH, 
			"%s@%s as %s: cmd='%s' cwd='%s'",
			remuser, hostname, locuser, cmdbuf, 
			cwd);
	}


	/* override USER, HOME, SHELL environment vars */
	putenv(username);
	putenv(homedir);
	putenv(shell);

	/* change to client working dir */
	if (chdir(cwd) < 0) {
		errlog("chdir to client working directory: %s", 
				strerror(errno));
		exit(1);
	}
	/*
	 * Close all fds, in case libc has left fun stuff like 
	 * /etc/shadow open.
	 */
	for (ifd = getdtablesize()-1; ifd > 2; ifd--) 
		close(ifd);

	execl(theshell, shellname, "-c", cmdbuf, 0);
	errlog("failed to exec shell: %s", strerror(errno));
	exit(1);
}

static void network_init(int fd, struct sockaddr_in *fromp)
{
	struct linger linger;
	socklen_t fromlen;
	int on=1;
	int port;

	fromlen = sizeof(*fromp);
	if (getpeername(fd, (struct sockaddr *) fromp, &fromlen) < 0) {
		syslog(LOG_ERR, "getpeername: %m");
		_exit(1);
	}
	if (keepalive &&
	    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (char *)&on,
	    sizeof(on)) < 0)
		syslog(LOG_WARNING, "setsockopt (SO_KEEPALIVE): %m");
	linger.l_onoff = 1;
	linger.l_linger = 60;			/* XXX */
	if (setsockopt(fd, SOL_SOCKET, SO_LINGER, (char *)&linger,
	    sizeof (linger)) < 0)
		syslog(LOG_WARNING, "setsockopt (SO_LINGER): %m");

	if (fromp->sin_family != AF_INET) {
	    syslog(LOG_ERR, "malformed \"from\" address (af %d)\n",
		   fromp->sin_family);
	    exit(1);
	}
#ifdef IP_OPTIONS
      {
	u_char optbuf[BUFSIZ/3], *cp;
	char lbuf[BUFSIZ+1], *lp;
	socklen_t optsize = sizeof(optbuf);
	int  ipproto;
	struct protoent *ip;

	if ((ip = getprotobyname("ip")) != NULL)
		ipproto = ip->p_proto;
	else
		ipproto = IPPROTO_IP;
	if (!getsockopt(0, ipproto, IP_OPTIONS, (char *)optbuf, &optsize) &&
	    optsize != 0) {
		lp = lbuf;

		/*
		 * If these are true, this will not run off the end of lbuf[].
		 */
		assert(optsize <= BUFSIZ/3);
		assert(3*optsize <= BUFSIZ);
		for (cp = optbuf; optsize > 0; cp++, optsize--, lp += 3)
			snprintf(lp, 4, " %2.2x", *cp);

		syslog(LOG_NOTICE,
		       "Connection received from %s using IP options"
		       " (ignored): %s",
		       inet_ntoa(fromp->sin_addr), lbuf);

		if (setsockopt(0, ipproto, IP_OPTIONS, NULL, optsize) != 0) {
			syslog(LOG_ERR, "setsockopt IP_OPTIONS NULL: %m");
			exit(1);
		}
	}
      }
#endif

	/*
	 * Check originating port for validity.
	 */
	port = ntohs(fromp->sin_port);
	if (port >= IPPORT_RESERVED || port < IPPORT_RESERVED/2) {
	    syslog(LOG_NOTICE|LOG_AUTH, "Connection from %s on illegal port",
		   inet_ntoa(fromp->sin_addr));
	    exit(1);
	}
}

int
main(int argc, char *argv[])
{
	int ch;
	struct sockaddr_in from;
	_check_rhosts_file=1;

	openlog("qshd", LOG_PID | LOG_ODELAY, LOG_DAEMON);

	opterr = 0;
	while ((ch = getopt(argc, argv, OPTIONS)) != EOF) {
		switch (ch) {
		case 'a':
			check_all = 1;
			break;

		case 'h':
			allow_root_rhosts = 1;
			break;

		case 'l':
			_check_rhosts_file = 0;
			break;

		case 'n':
			keepalive = 0;
			break;

		case 'L':
			paranoid = 1;
			break;

		case '?':
		default:
			syslog(LOG_ERR, "usage: qshd [-%s]", OPTIONS);
			exit(2);
		}
	}
	argc -= optind;
	argv += optind;

#ifdef USE_PAM
       if (_check_rhosts_file == 0 || allow_root_rhosts)
               syslog(LOG_ERR, "-l and -h functionality has been moved to "
                               "pam_rhosts_auth in /etc/pam.conf");
#endif /* USE_PAM */

	network_init(0, &from);
	doit(&from);
	return 0;
}





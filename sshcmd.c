/*
 * This is an rcmd() replacement originally by 
 * Chris Siebenmann <cks@utcc.utoronto.ca>.
 * 
 * Brought in to pdsh from USC rdist -jg
 * Changes:
 * - added fd2p arg handling
 * - changed name, func prototype, and added sshcmd() and rshcmd() wrappers
 * - MT-safe gethostent()
 * - use 'err' for output
 * - unset DISPLAY and call setsid() so ssh won't hang prompting for passphrase
 */

#if     HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <stdlib.h>	/* putenv */
#if	HAVE_UNISTD_H
#include <unistd.h>	
#endif
#if	HAVE_PTHREAD_H
#include <pthread.h>
#endif
#include <string.h>	/* memset */

#include "xstring.h"
#include "err.h"
#include "dsh.h"
#include "list.h"

#define HBUF_LEN	1024

/*
 * This is a replacement rcmd() function that uses an arbitrary
 * program in place of a direct rcmd() function call.
 */
static int 
pipecmd(char *path, char *args[], const char *ahost, int *fd2p)
{
	int             cpid;
	struct hostent  *hp;
	int             sp[2], esp[2];
#if HAVE_GETHOSTBYNAME_R
        struct hostent he;
        char hbuf[HBUF_LEN];
        int h_errnox;
        static pthread_mutex_t mylock = PTHREAD_MUTEX_INITIALIZER;
#endif
	/* validate remote hostname. */
#if HAVE_GETHOSTBYNAME_R
        memset(hbuf, 0, HBUF_LEN);
#ifdef __linux
        pthread_mutex_lock(&mylock);    /* RH 6.2 - race on /etc/hosts.conf? */
	/* linux compat args */
        (void)gethostbyname_r(ahost, &he, hbuf, HBUF_LEN, &hp, &h_errnox);
        pthread_mutex_unlock(&mylock);
#else 	/* solaris compat args */
        hp = gethostbyname_r(ahost, &he, hbuf, HBUF_LEN, &h_errnox);
#endif
#else
        hp = gethostbyname(ahost);      /* otherwise assumed MT-safe */
#endif					/* true on AIX4.3, OSF1 V4.0 */
	if (hp == NULL) {
		err("%p: gethostbyname: lookup of %S failed\n", ahost);
		return -1;
	}

	/* get a socketpair we'll use for stdin and stdout. */
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sp) < 0) {
		err("%p: socketpair failed for %S: %m\n", ahost);
		return -1;
	}
	if (fd2p) {
		if (socketpair(AF_UNIX, SOCK_STREAM, 0, esp) < 0) {
			err("%p: socketpair failed for %S: %m\n", ahost);
			return -1;
		}
	}

	cpid = fork();
	if (cpid < 0) {
		err("%p: fork failed for %S: %m\n", ahost);
		return -1;
	}
	if (cpid == 0) {
		/* child. we use sp[1] to be stdin/stdout, and close
		   sp[0]. */
		(void) close(sp[0]);
		if (dup2(sp[1], 0) < 0 || dup2(0,1) < 0) {
			err("%p: dup2 failed for %S: %m\n", ahost);
			_exit(255);
		}
		if (fd2p) {	/* stderr has own socketpair */
			close(esp[0]);
			if (dup2(esp[1], 2) < 0) {
				err("%p: dup2 failed for %S: %m\n", ahost);
				_exit(255);
			}
		} else {	/* stderr goes with stdout, stdin on sp[1] */
			if (dup2(0, 2) < 0) {
				err("%p: dup2 failed for %S: %m\n", ahost);
				_exit(255);
			}
		}
		/* heroic measures to prevent ssh from prompting for pphrase */
		setsid();
		putenv("DISPLAY=");
			
		/* fork again to lose parent. */
		cpid = fork();
		if (cpid < 0) {
			err("%p: fork to lose parent failed for %S: %m\n", ahost);
			_exit(255);
		}
		if (cpid > 0)
			_exit(0);
		/* in grandchild here. */
		execv(path, args);
		err("%p: execlp %s failed for %S: %m.", path, ahost);
		_exit(255);
	}
	if (cpid > 0) {
		/* parent. close sp[1], return sp[0]. */
		(void) close(sp[1]);
		if (fd2p) {
			close(esp[1]);
			*fd2p = esp[0];
		}
		/* reap child. */
		(void) wait(0);
		return sp[0];
	}
	/*NOTREACHED*/
	return 0;
}

int 
sshcmd(char *ahost, char *luser, char *ruser, char *cmd, int rank, int *fd2p)
{
	char *args[] = { "ssh", "-q", "-a", "-x", "-f", "-l", 0, 0, 0, 0 };

	args[6] = ruser;	/* solaris cc doesn't like non constant */
	args[7] = ahost; 	/*     initializers */
	args[8] = cmd;

	return pipecmd(_PATH_SSH, args, ahost, fd2p);
}

int 
sshcmdrw(char *ahost, char *luser, char *ruser, char *cmd, int rank, int *fd2p)
{
	char *args[] = { "ssh", "-q", "-a", "-x", "-l", 0, 0, 0, 0 };

	args[5] = ruser;	/* solaris cc doesn't like non constant */
	args[6] = ahost; 	/*     initializers */
	args[7] = cmd;

	return pipecmd(_PATH_SSH, args, ahost, fd2p);
}

void
sshcmd_init(opt_t *opt)
{
	/* not implemented */
}

void
sshcmd_signal(int fd, int signum)
{
	/* not implemented */
}

#if 0
int 
rshcmd(char *ahost, char *luser, char *ruser, char *cmd, int *fd2p)
{
	char *args[] = { "rsh", 0, "-l", 0, 0, 0 };

	args[1] = ahost;	/* solaris cc doesn't like non constant */
	args[3] = ruser; 	/*     initializers */
	args[4] = cmd;

	return pipecmd(_PATH_RSH, args, ahost, fd2p);
}
#endif

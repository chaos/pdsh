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

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include <sys/types.h>
#if HAVE_SYS_UIO_H
#  include <sys/uio.h>
#endif

#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <netdb.h>  /* rresvport */
#include <assert.h>
#include <string.h>
#include <errno.h>

#include "src/common/err.h"
#include "src/common/fd.h"

#define CONTROLLEN sizeof (struct cmsghdr) + sizeof (int)

static pthread_mutex_t privsep_mutex = PTHREAD_MUTEX_INITIALIZER;
static pid_t cpid;
static int client_fd = -1;
static int server_fd = -1;

uid_t user_uid = -1;
gid_t user_gid = -1;
uid_t priv_uid = -1;
gid_t priv_gid = -1;

/*
 *  Top 16 bits of port sent to privsep server is used to encode
 *   the address family (for rresvport_af()). Currently, value is
 *   either AF_INET AF_INET6 (if zero, then AF_INET).
 */
static int privsep_get_family (int *lport)
{
	int family = (*lport>>16) & 0xffff;
	/*
	 *  Mask out family from lport:
	 */
	*lport &= 0xffff;

	return (family ? family : AF_INET);
}

static int privsep_set_family (int *lport, int family)
{
	if (family > 0xffff)
		return (-1);
	*lport |= (family<<16);
	return (0);
}

static int create_socketpair (void)
{
	int pfds[2];

	if (socketpair (AF_UNIX, SOCK_STREAM, 0, pfds) < 0) {
		err ("%p: socketpair failed in privilege separation: %m\n");
		return -1;
	}

	client_fd = pfds[0];
	server_fd = pfds[1];

	return (0);
}

static void drop_privileges (void)
{
	user_uid = getuid ();
	priv_uid = geteuid ();
	user_gid = getgid ();
	priv_gid = getegid ();

#ifdef _POSIX_SAVED_IDS
	seteuid (user_uid);
	setegid (user_gid);
#else
	setreuid (priv_uid, user_uid);
	setregid (priv_gid, user_gid);
#endif
}

static int send_rresvport (int pipefd, int fd, int lport)
{
	struct iovec   iov[1];
	struct msghdr  msg;
#if !HAVE_MSGHDR_ACCRIGHTS
	struct cmsghdr *cmsg;
	char *         buf[CONTROLLEN];

	cmsg = (struct cmsghdr *) &buf;
#endif
#if !HAVE_RRESVPORT
	errno = ENOSYS;
	return (-1);
#endif

	memset (&msg, 0, sizeof (msg));

	iov->iov_base  = (void *) &lport;
	iov->iov_len   = sizeof (lport);
	msg.msg_iov    = iov;
	msg.msg_iovlen = 1;

#if HAVE_MSGHDR_ACCRIGHTS
	if (fd < 0) {
		msg.msg_accrights = NULL;
		msg.msg_accrightslen = 0;
	} else {
		msg.msg_accrights = (caddr_t) &fd;
		msg.msg_accrightslen = sizeof (int);
	}
#else 
	if (fd < 0) {
		msg.msg_control = NULL;
		msg.msg_controllen = 0;
		lport = -1;
	} else {
		cmsg->cmsg_level   = SOL_SOCKET;
		cmsg->cmsg_type    = SCM_RIGHTS;
		cmsg->cmsg_len     = CONTROLLEN;
		msg.msg_control    = (caddr_t) cmsg;
		msg.msg_controllen = CONTROLLEN;
		* (int *) CMSG_DATA(cmsg) = fd;
	}
#endif

	if (sendmsg (pipefd, &msg, 0) != sizeof (int)) {
		err ("%p: privsep: sendmsg: %m\n");
		return (-1);
	}

	return (0);
}

#if HAVE_RRESVPORT
static int recv_rresvport (int pipefd, int *lptr)
{
	int            fd = -1;
	struct iovec   iov[1];
	struct msghdr  msg;
#if !HAVE_MSGHDR_ACCRIGHTS
	struct cmsghdr *cmsg;
	char *         buf[CONTROLLEN];

	cmsg = (struct cmsghdr *) &buf;
#endif
	memset (&msg, 0, sizeof (msg));

	iov->iov_base  = (void *) lptr;
	iov->iov_len   = sizeof (int);
	msg.msg_iov    = iov;
	msg.msg_iovlen = 1;

#if HAVE_MSGHDR_ACCRIGHTS
	msg.msg_accrights = (caddr_t) &fd;
	msg.msg_accrightslen = sizeof (int);
#else /* !HAVE_MSGHDR_ACCRIGHTS */
	cmsg->cmsg_level   = SOL_SOCKET;
	cmsg->cmsg_type    = SCM_RIGHTS;
	cmsg->cmsg_len     = CONTROLLEN;
	msg.msg_control    = (caddr_t) cmsg;
	msg.msg_controllen = CONTROLLEN;
#endif

	if (recvmsg (pipefd, &msg, 0) < 0)
		err ("%p: privsep: recvmsg: %m\n");

	if (*lptr < 0)
		return (-1);

#if !HAVE_MSGHDR_ACCRIGHTS
	fd = *(int *) CMSG_DATA (cmsg);
#endif

	return (fd);
}
#endif /* HAVE_RRESVPORT */


static int p_rresvport_af (int *port, int family)
{
#if HAVE_RRESVPORT_AF
	return rresvport_af (port, family);
#elif HAVE_RRESVPORT
	/*  Family must be AF_INET
	 */
	if (family != AF_INET)
		err ("%p: rresvport called with family != AF_INET!\n");
	/* ignore family != AF_INET */
	return rresvport (port);
#else
	errno = ENOSYS;
	return (-1);
#endif
}

static int privsep_server (void)
{
	int rc;
	int lport;
	close (client_fd);

	/*
	 * for each request on server_fd create a reserved port and
	 *   send the created fd back to the client.
	 */
	while ((rc = read (server_fd, &lport, sizeof (lport))) > 0) {
		int family = privsep_get_family (&lport);
		int s = p_rresvport_af (&lport, family);

		send_rresvport (server_fd, s, lport);

		close (s);
	}

	if (rc < 0)
		err ("%p: privsep: server read failed: %m\n");

	close (server_fd);

	return (0);

}

static int create_privileged_child (void)
{
	if ((cpid = fork ()) < 0) {
		err ("%p: fork failed in privilege separation: %m\n");
		return -1;
	}

	if (cpid == 0) {
		/* 
		 * Child: become privilege port server.
		 */
		privsep_server ();
		exit (0);
	}

	/*
	 * Parent: drop privileges, close server_fd and return.
	 */
	close (server_fd);

	drop_privileges ();

	return (0);
}

int privsep_init (void)
{
#if !HAVE_RRESVPORT
	return (0);
#endif
	if (geteuid() == getuid())
		return 0;

	if (create_socketpair () < 0)
		return -1;

	return (create_privileged_child ());
}

int privsep_fini (void)
{
#if !HAVE_RRESVPORT
	return (0);
#endif
	int status;
	if (client_fd < 0 || cpid < 0)
		return (0);

	close (client_fd);

	if (waitpid (cpid, &status, 0) < 0) {
		err ("%p: failed to reap priveleged child: %m\n");
		return (-1);
	}

	if (status) 
		err ("%p: privileged chiled exited with status %d\n", status);

	return (0);
}

int privsep_rresvport_af (int *lport, int family)
{
	int s = -1;

	if (client_fd < 0)
		return (p_rresvport_af (lport, family));

	if (privsep_set_family (lport, family) < 0) {
		err ("%p: privsep_rresvport_af: Invalid family %d\n", family);
		errno = EINVAL;
		return (-1);
	}

	if ((errno = pthread_mutex_lock (&privsep_mutex)))
		errx ("%p: %s:%d: mutex_lock: %m\n", __FILE__, __LINE__);

	if (write (client_fd, lport, sizeof (*lport)) < 0) {
		err ("%p: privsep: client write: %m\n");
		goto out;
	}

	s = recv_rresvport (client_fd, lport);

out:
	if ((errno = pthread_mutex_unlock (&privsep_mutex)))
		errx ("%p: %s:%d: mutex_unlock: %m\n", __FILE__, __LINE__);

	return (s);
}

int privsep_rresvport (int *lport)
{
#if HAVE_RRESVPORT
	return privsep_rresvport_af (lport, AF_INET);
#else
	errno = ENOSYS;
	return -1;
#endif /* HAVE_RRESVPORT */
	
}

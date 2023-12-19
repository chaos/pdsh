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

#if HAVE_POLL_H
#include <poll.h>
#else
#if HAVE_SYS_POLL_H
#include <sys/poll.h>
#endif /* HAVE_SYS_POLL_H */
#endif /* HAVE_POLL_H */

#include <sys/types.h>
#include <sys/select.h>
#include <sys/time.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "xpoll.h"
#include "xmalloc.h"

#if HAVE_POLL
static int
_poll(struct xpollfd *xfds, unsigned int nfds, int timeout) {
    int i, rv;
    struct pollfd *pfds = Malloc(nfds * sizeof(struct pollfd));

    for (i = 0; i < nfds; i++) {
        pfds[i].fd = xfds[i].fd;
        pfds[i].events = 0;
        pfds[i].revents = 0;

        if (xfds[i].events & XPOLLREAD)
            pfds[i].events |= POLLIN;
        if (xfds[i].events & XPOLLWRITE)
            pfds[i].events |= POLLOUT;
    }

    if ((rv = poll(pfds, nfds, timeout)) < 0) {
        Free((void **)&pfds);
        return -1;
    }

    for (i = 0; i < nfds; i++) {
        if (pfds[i].revents & POLLIN)
            xfds[i].revents |= XPOLLREAD;
        if (pfds[i].revents & POLLOUT)
            xfds[i].revents |= XPOLLWRITE;
        if (pfds[i].revents & POLLERR || pfds[i].revents & POLLHUP)
            xfds[i].revents |= XPOLLERR;
        if (pfds[i].revents & POLLNVAL)
            xfds[i].revents |= XPOLLINVAL;
    }

    Free((void **)&pfds);
    errno = 0;
    return rv;
}

#else /* !HAVE_POLL */

static int
_select(struct xpollfd *xfds, unsigned int nfds, int timeout) {
    int i, maxfd = -1, inval = 0, rv = -1;
    struct timeval tv;
    struct timeval *tptr = &tv;
    fd_set reads, writes;

    if (timeout < 0)
        tptr = NULL;
    else {
        tv.tv_sec = timeout;
        tv.tv_usec = 0;
    }

    /* setup for select() */
    FD_ZERO(&reads);
    FD_ZERO(&writes);
    for (i = 0; i < nfds; i++) {
        if (xfds[i].fd >= FD_SETSIZE || xfds[i].fd < 0) {
            xfds[i].revents |= XPOLLINVAL;
            inval++;
            continue;
        }
        if (xfds[i].events & XPOLLREAD)
            FD_SET(xfds[i].fd, &reads);
        if (xfds[i].events & XPOLLWRITE)
            FD_SET(xfds[i].fd, &writes);
        if (xfds[i].fd > maxfd)
            maxfd = xfds[i].fd;
    }

    while (rv == -1) {
        if ((rv = select(maxfd + 1, &reads, &writes, NULL, tptr)) < 0) {
            if (errno != EBADF)
                return -1;
            else {
                /* check for and remove bad fds */
                struct timeval ttv = {0, 1};  /* very very short timeout */
                fd_set rds, wrs;

                maxfd = -1;
                FD_ZERO(&reads);
                FD_ZERO(&writes);
                for (i = 0; i < nfds; i++) {
                    if (xfds[i].revents & XPOLLINVAL)
                        continue;

                    FD_ZERO(&rds);
                    FD_ZERO(&wrs);
                    if (xfds[i].events & XPOLLREAD)
                        FD_SET(xfds[i].fd, &rds);
                    if (xfds[i].events & XPOLLWRITE)
                        FD_SET(xfds[i].fd, &wrs);

                    if (select(xfds[i].fd + 1, &rds, &wrs, NULL, &ttv) < 0) {
                        if (errno != EBADF)
                            return -1;
                        else {
                            xfds[i].revents |= XPOLLINVAL;
                            inval++;
                        }
                    }
                    else {
                        /* prepare for next select */
                        if (xfds[i].events & XPOLLREAD)
                            FD_SET(xfds[i].fd, &reads);
                        if (xfds[i].events & XPOLLWRITE)
                            FD_SET(xfds[i].fd, &writes);
                        if (xfds[i].fd > maxfd)
                            maxfd = xfds[i].fd;
                    }
                }
            }
        }
    }

    for (i = 0; i < nfds; i++) {
        /* protect segfault prone FD_ISSET */
        if (xfds[i].revents & XPOLLINVAL)
            continue;

        if (FD_ISSET(xfds[i].fd, &reads))
            xfds[i].revents |= XPOLLREAD;
        if (FD_ISSET(xfds[i].fd, &writes))
            xfds[i].revents |= XPOLLWRITE;
    }

    errno = 0;
    return (rv + inval);
}
#endif /* HAVE_POLL */

int xpoll(struct xpollfd *xfds, int nfds, int timeout) {
    int i;

    errno = 0;

    if (xfds == NULL || nfds <= 0) {
        errno = EINVAL;
        return -1;
    }

    for (i = 0; i < nfds; i++) {
        xfds[i].revents = 0;
    }

#if HAVE_POLL
    return _poll(xfds, nfds, timeout);
#else
    return _select(xfds, nfds, timeout);
#endif
}

/*
 * vi: tabstop=4 shiftwidth=4 expandtab
 */

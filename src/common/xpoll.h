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

#ifndef _XPOLL_H
#define _XPOLL_H

/* Input & Result events */
#define  XPOLLREAD    0x0001   /* fd can be read */
#define  XPOLLWRITE   0x0002   /* fd can be written to */

/* Result only events */
#define  XPOLLINVAL   0x0010   /* invalid fd passed in */
#define  XPOLLERR     0x0020   /* error occurred on this fd */

struct xpollfd {
    int fd;          /* file descriptor to check */
    short events;    /* events to check for */
    short revents;   /* resulting events occuring on this fd */
};

/*
 * xpoll()
 * - Wrapper API around poll() and select().  
 * 
 * Input:
 * xfds - pointer to array of xfds structures
 * nfds - number of structures in xfds array
 * timeout - timeout length to poll or select.
 *    if timeout < 0  - poll infinitely
 *    if timeout == 0 - return immediately
 *    if timeout > 0  - poll this number of seconds
 *
 * Output:
 * Number of file descriptors in which revents is modified.  On error,
 * -1 is returned and errno set to the appropriate error
 *
 * Additional Notes:
 * - Any invalid bits stored in events are ignored
 * - revents is cleared before any revents are set
 * - XPOLLINVAL and XPOLLERR are not mutually exclusive with
 *   XPOLLREAD and XPOLLWRITE.  
 */ 
int xpoll(struct xpollfd *xfds, int nfds, int timeout);

#endif /* _XPOLL_H */

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

#ifndef _MACROS_INCLUDED
#define _MACROS_INCLUDED

#if 	HAVE_CONFIG_H
#include "config.h"
#endif

#if HAVE_PTHREAD_H
#include <pthread.h>
#endif

#define LINEBUFSIZE     	2048

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 64
#endif

#ifndef MAX
#define MAX(a,b)	((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a,b)	((a) < (b) ? (a) : (b))
#endif

#define IP_ADDR_LEN	4       /* XXX */

#if !HAVE_PTHREAD_SIGMASK && HAVE_SIGTHREADMASK
#define pthread_sigmask(x, y, z)	sigthreadmask(x, y, z)
#endif

#ifndef _BOOL_DEFINED
#define _BOOL_DEFINED
typedef enum { false, true } bool;
#endif

#endif                          /* !_MACROS_INCLUDED */

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

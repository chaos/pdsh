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

#ifndef _HAVE_RCMD_H
#define _HAVE_RCMD_H

#include "opt.h"

struct rcmd_info {
	int fd;
	int efd;
	struct rcmd_module *rmod;
	void *arg;
};

/*
 *  Register a new default rcmd connect module for the hosts in
 *   string "hosts".
 */
int rcmd_register_default_module (char *hosts, char *rcmd_type);

/*
 *  Return default rcmd module name.
 */
char * rcmd_get_default_module (void);

/*
 *  Create rcmd connect info structure for a given host
 */
struct rcmd_info * rcmd_create (char *host, char *addr, char *locuser, 
		                        char *remuser, char *cmd, int nodeid, bool err);

/*
 *  Destroy rcmd connections
 */
int rcmd_destroy (struct rcmd_info *);

/*
 *  Send a signal of the specified remote connection
 */
int rcmd_signal (struct rcmd_info *, int signum);

int rcmd_init (opt_t *opt);

/*
 *  Free all rcmd module information.
 */
int rcmd_exit (void);

#endif /* !_HAVE_RCMD_H */

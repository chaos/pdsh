/*****************************************************************************\
 *  $Id$
 *****************************************************************************
 *  Copyright (C) 2001-2006 The Regents of the University of California.
 *  Produced at Lawrence Livermore National Laboratory (cf, DISCLAIMER).
 *  Written by Mark Grondona <mgrondona@llnl.gov>.
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

struct rcmd_options {
	bool resolve_hosts;
};

#define RCMD_OPT_RESOLVE_HOSTS 0x1

struct rcmd_info {
	int                   fd;
	int                   efd;
	struct rcmd_module   *rmod;
	struct rcmd_options  *opts;
	char                 *ruser;
	void                 *arg;
};


/*
 *  Register default rcmd parameters for hosts in hostlist string "hosts."
 *    rcmd_type - if non-NULL set default rcmd connect module for "hosts."
 *    user      - if non-NULL set default remote username for "hosts."
 *
 *  The first call to this function "wins," i.e. later calls to register
 *   will not override existing defaults. This is done because currently
 *   in pdsh, command line options are processed *before* configuration
 *   type files (i.e. genders) since these files are processed by pdsh
 *   modules.
 */
int rcmd_register_defaults (char *hosts, char *rcmd_type, char *user);

/*
 *  Register default rcmd type
 */
int rcmd_register_default_rcmd (char *rcmd_name);

/*
 *  Return default rcmd module name.
 */
char * rcmd_get_default_module (void);


/*
 *  Create and rcmd_info object for specified host
 */
struct rcmd_info * rcmd_create (char *host);

/*
 *  Connect using rcmd_info rcmd
 */
int rcmd_connect (struct rcmd_info *rcmd, char *host, char *addr,
                  char *locuser, char *remuser, char *cmd, int nodeid,
		  bool err);

/*
 *  Destroy rcmd connections
 */
int rcmd_destroy (struct rcmd_info *);

/*
 *  Send a signal over the specified remote connection
 */
int rcmd_signal (struct rcmd_info *, int signum);

int rcmd_init (opt_t *opt);

/*
 *  Free all rcmd module information.
 */
int rcmd_exit (void);

/*
 *  Called by rcmd module during "init" function to set various
 *   rcmd-specific options. (see rcmd_options structure above)
 *
 *  Returns -1 with errno set to ESRCH if called from anywhere but
 *   module's rcmd_init function.
 */
int rcmd_opt_set (int id, void * value);

#endif /* !_HAVE_RCMD_H */

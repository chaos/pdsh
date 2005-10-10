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
#ifndef _HAVE_MOD_RCMD_H
#define _HAVE_MOD_RCMD_H

struct rcmd_info {
    int fd;
    int efd;
    void *arg;
};

/*
 *  Get the default rcmd module name (e.g. "rsh" "ssh" etc.)
 */
char * mod_rcmd_get_default_module(void);

/*
 *  Load the chosen or default rcmd module
 */
int    mod_rcmd_load(opt_t *opt);

/*
 *  Perform rcmd initialization for the loaded rcmd module
 */
int    mod_rcmd_init(opt_t *opt);

/*
 *  Send a signal over the specified file descriptor
 */
int    mod_rcmd_signal(struct rcmd_info *, int signum);

/*
 *  Spawn a remote command using the loaded rcmd module
 */
struct rcmd_info * mod_rcmd_create (char *ahost, char *addr, char *locuser, 
                            char *remuser, char *cmd, int rank, bool);

/*
 *  Destroy rcmd connections
 */
int mod_rcmd_destroy (struct rcmd_info *);


/*
 *  Clean up rcmd modules
 */
void mod_rcmd_exit(void);

#endif /* !_HAVE_MOD_RCMD_H */

/*****************************************************************************\
 *  $Id$
 *****************************************************************************
 *  Copyright (C) 2007 The Regents of the University of California.
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

#ifndef _HAVE_PIPECMD_H
#define _HAVE_PIPECMD_H

typedef struct pipe_info_struct * pipecmd_t;

/*
 *  Run a command with stdout/in and stderr connected to
 *   pipes available to pdsh.
 *  Args are re-formatted replacing %h, %u, and %n with
 *   remote "host", user, and "rank" respectively.
 * 
 *  Returns NULL pipecmd object on failure.
 */
pipecmd_t pipecmd (const char *path, const char **args, 
        const char *target, const char *user, int rank);

/*
 *  Destroy pipecmd object - freeing associated memory
 */
void pipecmd_destroy (pipecmd_t p);

/*
 *  Return file descriptors for pipecmd object
 */
int pipecmd_stdoutfd (pipecmd_t p);
int pipecmd_stderrfd (pipecmd_t p);

/*
 *  Send signal [signo] to child process associtated with given
 *   pipecmd object
 */
int pipecmd_signal (pipecmd_t p, int signo);

/*
 *  Wait for and reap pipecmd child process. Returns exit status
 *   in status if it is non-NULL
 */
int pipecmd_wait (pipecmd_t p, int *status);

/*
 *  Return target name of pipecmd process (i.e. target host)
 */
const char * pipecmd_target (pipecmd_t p);

#endif /* !_HAVE_PIPECMD_H */

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

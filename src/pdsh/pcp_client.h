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

#ifndef _PCP_CLIENT_H
#define _PCP_CLIENT_H

#if HAVE_CONFIG_H
#  include <config.h>
#endif 

#include "src/pdsh/opt.h"

#include "src/common/list.h"

/* define the filename flag as an impossible filename */
#define EXIT_SUBDIR_FILENAME    "a!b@c#d$"
#define EXIT_SUBDIR_FLAG        "E\n"

/* Store the file that should be copied and if it was a
 * file specified by the user or if it is a file found due to
 * recursively moving down a directory (-r option).  This flag
 * is needed so the right output filename can be determined
 * on reverse copies.
 */
struct pcp_filename {
    char *filename;
    int file_specified_by_user;
};

/* expand directories, if any, and verify access for all files */
List pcp_expand_dirs (List infile_names);

struct pcp_client {
	int infd;
	int outfd;
	bool preserve;
	bool pcp_client;
	char *host;
	List infiles;
};

int pcp_client (struct pcp_client *cli);

#endif /* _PCP_CLIENT_H */

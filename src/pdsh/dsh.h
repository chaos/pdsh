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

#ifndef _DSH_INCLUDED
#define _DSH_INCLUDED

#if 	HAVE_CONFIG_H
#include "config.h"
#endif

#if HAVE_PTHREAD_H
#include <pthread.h>
#endif

#include "src/common/macros.h"
#include "src/common/list.h"
#include "src/pdsh/opt.h"

#define INTR_TIME		1       /* secs */
#define WDOG_POLL 		2       /* secs */

/* some handy SP constants */
/* NOTE: degenerate case of one node per frame, nodes would be 1, 17, 33,... */
#define MAX_SP_NODES 		512
#define MAX_SP_NODES_PER_FRAME	16
#define MAX_SP_NODE_NUMBER (MAX_SP_NODES * MAX_SP_NODES_PER_FRAME - 1)

typedef enum { DSH_NEW, DSH_RCMD, DSH_READING, DSH_DONE,
        DSH_FAILED } state_t;

typedef struct thd {
    pthread_t thread;
    pthread_attr_t attr;
    state_t state;              /* thread state */
    char *host;                 /* host name */
    char *luser;                /* local username */
    char *ruser;                /* remote username */
    bool resolve_hosts;         /* resolve hosts in thread? */
    time_t start;               /* time stamp for start */
    time_t connect;             /* time stamp for connect */
    time_t finish;              /* time stamp for finish */
    char *cmd;                  /* command */

    bool dsh_sopt;              /* true if -s (sep stderr/out) */

    bool kill_on_fail;          /* If true, kill all procs on single failure */

    List pcp_infiles;           /* name of input files/dirs */
    char *pcp_outfile;          /* name of output file/dir */
    bool pcp_popt;              /* preserve mtime/mode */
    bool pcp_ropt;              /* recursive */
    char *pcp_progname;         /* program name */
    int rc;                     /* remote return code (-S) */
    int nodeid;                 /* node index */
    int nnodes;                 /* number of nodes in job */
    int fd;                     /* stdin/stdout */
    int efd;                    /* signal/stderr */
    bool labels;                /* display host: labels */
    char addr[IP_ADDR_LEN];     /* IP address */
} thd_t;

int dsh(opt_t *);
void set_rcmd_timeout(int);
void testcase(int);

#endif                          /* _DSH_INCLUDED */

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

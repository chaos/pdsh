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

#if HAVE_CONFIG_H
#  include "config.h"
#endif

#include "wcoll.h"
#include "mod.h"

#if STATIC_MODULES
#  define pdsh_module_info machines_module_info
#endif    

static hostlist_t read_machines(opt_t *opt);
static int machines_opt_a(opt_t *, int, char *);

static bool allnodes = false;

/* 
 * Export pdsh module operations structure
 */
struct pdsh_module_operations machines_module_ops = {
    (ModInitF)       NULL, 
    (ModExitF)       NULL, 
    (ModReadWcollF)  read_machines, 
    (ModPostOpF)     NULL,
};

/* 
 * Export rcmd module operations
 */
struct pdsh_rcmd_operations machines_rcmd_ops = {
    (RcmdInitF)  NULL,
    (RcmdSigF)   NULL,
    (RcmdF)      NULL,
};

/* 
 * Export module options
 */
struct pdsh_module_option machines_module_options[] = 
 { { 'a', NULL, "target all nodes", (optFunc) machines_opt_a },
   PDSH_OPT_TABLE_END
 };

/* 
 * Machines module info 
 */
struct pdsh_module pdsh_module_info = {
  "misc",
  "machines",
  "Jim Garlick <garlick@llnl.gov>",
  "Read list of all nodes from a machines file",

  &machines_module_ops,
  &machines_rcmd_ops,
  &machines_module_options[0],
};

static int machines_opt_a(opt_t *pdsh_opt, int opt, char *arg)
{
    allnodes = true;
    return 0;
}

static hostlist_t read_machines(opt_t *opt)
{
    if (!allnodes)
        return NULL;

    if (opt->wcoll)
        errx("Do not specify both -w and -a");

    return read_wcoll(_PATH_MACHINES, NULL);
}

/*
 * vi: tabstop=4 shiftwidth=4 expandtab
 */

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

#include <stdio.h>
#include <stdlib.h>
#include <nodeupdown.h>

#include "src/common/hostlist.h"
#include "src/common/err.h"
#include "src/common/xmalloc.h"
#include "src/pdsh/mod.h"

#ifndef GENDERS_ALTNAME_ATTRIBUTE
#  define GENDERS_ALTNAME_ATTRIBUTE   "altname"
#endif

#if STATIC_MODULES
#  define pdsh_module_info nodeupdown_module_info
#  define pdsh_module_priority nodeupdown_module_priority
#endif    

int pdsh_module_priority = 120;

static int mod_nodeupdown_postop(opt_t *opt);
static int nodeupdown_opt_v(opt_t *, int, char *);
static void remove_all_down_nodes(hostlist_t);

static bool verify = false;

/* 
 * Export pdsh module operations structure
 */
struct pdsh_module_operations nodeupdown_module_ops = {
    (ModInitF)       NULL,
    (ModExitF)       NULL,
    (ModReadWcollF)  NULL,
    (ModPostOpF)     mod_nodeupdown_postop
};

/* 
 * Export rcmd module operations
 */
struct pdsh_rcmd_operations nodeupdown_rcmd_ops = {
    (RcmdInitF)  NULL,
    (RcmdSigF)   NULL,
    (RcmdF)      NULL,
};

/* 
 * Export module options
 */
struct pdsh_module_option nodeupdown_module_options[] = 
 { { 'v', NULL, "exclude targets if they are down", 
     DSH | PCP, (optFunc) nodeupdown_opt_v
   },
   PDSH_OPT_TABLE_END
 };

/* 
 * Nodeupdown module info 
 */
struct pdsh_module pdsh_module_info = {
  "misc",
  "nodeupdown",
  "Al Chu <chu11@llnl.gov>",
  "remove targets if down according to libnodeupdown",
  DSH | PCP, 

  &nodeupdown_module_ops,
  &nodeupdown_rcmd_ops,
  &nodeupdown_module_options[0],
};

static int nodeupdown_opt_v(opt_t *pdsh_opt, int opt, char *arg)
{
    verify = true;
    return 0;
}

static int mod_nodeupdown_postop(opt_t *opt)
{
    if (!verify || !opt->wcoll)
        return 0;

    remove_all_down_nodes(opt->wcoll);

    return 0;
}

/*
 *  Remove down nodes from hostlist wcoll using "nodeupdown_is_down_node"
 *    on each member of wcoll. Supposedly, it doesn't matter whether you
 *    pass in the canonical or altname.
 */
static void
remove_all_down_nodes(hostlist_t wcoll)
{
    nodeupdown_t  nh   = NULL;
    char *        host = NULL;
    hostlist_iterator_t i = NULL;
    
    if ((nh = nodeupdown_handle_create()) == NULL)
        errx("%p: Unable to create nodeupdown handle.\n");

#if HAVE_NODEUPDOWN_LOAD_DATA_6
    if (nodeupdown_load_data(nh, NULL, NULL, NULL, 0, 0) < 0) 
#else
    if (nodeupdown_load_data(nh, NULL, 0, 0, NULL) < 0) 
#endif
        errx("%p: nodeupdown: %s\n", nodeupdown_errormsg(nh));

    i = hostlist_iterator_create(wcoll);
    while ((host = hostlist_next(i))) {
        if (nodeupdown_is_node_down(nh, host) > 0)
            hostlist_remove(i);
        free(host);
    }
    hostlist_iterator_destroy(i);

    if (nodeupdown_handle_destroy(nh) < 0)
        err("%p: nodeupdown_handle_destroy: %s\n", nodeupdown_errormsg(nh));

    return;
}


/*
 * vi: tabstop=4 shiftwidth=4 expandtab
 */

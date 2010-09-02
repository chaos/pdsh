/*****************************************************************************\
 *  $Id$
 *****************************************************************************
 *  Copyright (C) 2005-2006 The Regents of the University of California.
 *  Produced at Lawrence Livermore National Laboratory (cf, DISCLAIMER).
 *  Written by Mark Grondona <mgrondona@llnl.gov>
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

#include <stdlib.h> /* getenv */
#include <string.h>
#include <netdb.h>

#include "src/pdsh/wcoll.h"
#include "src/pdsh/mod.h"
#include "src/common/hostlist.h"
#include "src/common/xmalloc.h"
#include "src/common/err.h"
#include "src/common/list.h"
#include "src/common/split.h"

#if STATIC_MODULES
#  define pdsh_module_info netgroup_module_info
#  define pdsh_module_priority netgroup_module_priority
#endif    

int pdsh_module_priority = DEFAULT_MODULE_PRIORITY;

static hostlist_t read_netgroup(opt_t *opt);
static int netgroup_postop (opt_t *);
static int netgroup_process_opt(opt_t *, int, char *);

static List groups = NULL;
static List exgroups = NULL;

/* 
 * Export pdsh module operations structure
 */
struct pdsh_module_operations netgroup_module_ops = {
    (ModInitF)       NULL, 
    (ModExitF)       NULL, 
    (ModReadWcollF)  read_netgroup, 
    (ModPostOpF)     netgroup_postop,
};

/* 
 * Export rcmd module operations
 */
struct pdsh_rcmd_operations netgroup_rcmd_ops = {
    (RcmdInitF)  NULL,
    (RcmdSigF)   NULL,
    (RcmdF)      NULL,
};

/* 
 * Export module options
 */
struct pdsh_module_option netgroup_module_options[] = 
 { { 'g', "groupname", "target hosts in netgroup \"groupname\"", 
   DSH | PCP, (optFunc) netgroup_process_opt },
   { 'X', "groupname", "exclude hosts in netgroup \"groupname\"", 
   DSH | PCP, (optFunc) netgroup_process_opt },
   PDSH_OPT_TABLE_END
 };

/*
 * Machines module info 
 */
struct pdsh_module pdsh_module_info = {
  "misc",
  "netgroup",
  "Mark Grondona <mgrondona@llnl.gov>",
  "Target netgroups from pdsh",
  DSH | PCP, 
  &netgroup_module_ops,
  &netgroup_rcmd_ops,
  &netgroup_module_options[0],
};

static int netgroup_process_opt(opt_t *pdsh_opt, int opt, char *arg)
{
    switch (opt) {
    case 'g':
        groups = list_split_append (groups, ",", arg);
        break;
    case 'X':
        exgroups = list_split_append (exgroups, ",", arg);
        break;
    default:
        err ("%p: netgroup_process_opt: invalid option `%c'\n", opt);
        return -1;
        break;
    }
    return 0;
}

static hostlist_t _read_netgroup (const char *group)
{
	hostlist_t hl = NULL;
	char *host, *user, *domain;
	char buf[4096];
    int rc;

	setnetgrent (group);

	while (rc = getnetgrent_r (&host, &user, &domain, buf, sizeof (buf))) {
		if (hl == NULL)
			hl = hostlist_create (host);
		else
			hostlist_push (hl, host);
	}

	endnetgrent ();

	return (hl);
}

static hostlist_t _read_groups (List grouplist)
{
    ListIterator i  = NULL;
    hostlist_t   hl = NULL;
    char *group;

    i = list_iterator_create (grouplist);

    while ((group = list_next (i))) {
        hostlist_t l = _read_netgroup (group);

        if (l == NULL)
            continue;
    
        if (hl == NULL) {
            hl = l;
        } else {
            hostlist_push_list (hl, l);
            hostlist_destroy (l);
        }
    }

    list_iterator_destroy (i);

    if (hl != NULL)
        hostlist_uniq (hl);

    return (hl);
}

static hostlist_t read_netgroup (opt_t *opt)
{
    if (!groups)
        return NULL;

    if (opt->wcoll && groups)
        errx("Do not specify both -w and -g");

    return _read_groups (groups);
}

static int
_delete_all (hostlist_t hl, hostlist_t dl)
{
    int                 rc   = 0;
    char *              host = NULL;
    hostlist_iterator_t i    = hostlist_iterator_create (dl);

    while ((host = hostlist_next (i))) {
        rc += hostlist_delete_host (hl, host);
        free (host);
    }
    hostlist_iterator_destroy (i);
    return (rc);
}

static int netgroup_postop (opt_t *opt)
{
    hostlist_t hl = NULL;
    
    if (!opt->wcoll || !exgroups)
        return (0);

    if ((hl = _read_groups (exgroups)) == NULL)
        return (0);

    _delete_all (opt->wcoll, hl);

    return 0;
}

/*
 * vi: tabstop=4 shiftwidth=4 expandtab
 */

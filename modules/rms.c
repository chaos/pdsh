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
#include <string.h>
#include <qsnet/types.h>
#include <rms/rmsapi.h>

#include "hostlist.h"
#include "err.h"
#include "xmalloc.h"
#include "xpopen.h"
#include "xstring.h"
#include "mod.h"

MODULE_TYPE        ( "misc"                                              );
MODULE_NAME        ( "rms"                                               );
MODULE_AUTHOR      ( "Jim Garlick <garlick@llnl.gov>"                    );
MODULE_DESCRIPTION ( "Attempt to read wcoll from RMS_RESOURCEID env var" );

/*
 *  Export generic pdsh module options
 */
static int mod_rms_postop(opt_t *opt);

struct pdsh_module_operations pdsh_module_ops = {
    NULL,
    NULL,
    NULL,
    (ModPostOpF) mod_rms_postop
};


static hostlist_t _rms_rid_to_nodes(char *part, int rid);
static hostlist_t _rms_wcoll(void);


/*
 *  If no wcoll has been established by this time, look for the
 *    RMS_RESOURCE env var, and set wcoll to the list of nodes allocated
 *    to that resource.
 */
static int mod_rms_postop(opt_t *opt)
{
    if (opt->wcoll)
        return 0;

    opt->wcoll = _rms_wcoll();

    return 0;
}

/* 
 * Helper for rms_wcoll() - RMS provides no API to get the list of nodes 
 * once allocated, so we query the msql database with 'rmsquery'.
 * part (IN)            partition name
 * rid (IN)             resource id
 * result (RETURN)      NULL or a list of hostnames
 */
static hostlist_t _rms_rid_to_nodes(char *part, int rid)
{
    FILE *f;
    char tmp[256];

    /* XXX how to specify partition?  do we need to? */
    snprintf(tmp, sizeof(tmp),
             "%s \"select hostnames from resources where name='%d'\"",
             _PATH_RMSQUERY, rid);
    f = xpopen(tmp, "r");
    if (f == NULL)
        errx("%p: error running %s\n", _PATH_RMSQUERY);
    *tmp = '\0';
    while (fgets(tmp, sizeof(tmp), f) != NULL);
    xpclose(f);
    /* should either have empty string or host[n-m] range */
    /* turn elanid range into list of hostnames */
    xstrcln(tmp, "\r\n\t ");    /* drop trailing \n */
    return hostlist_create(tmp);
}

/*
 * If RMS_RESOURCE is set, return wcoll corresponding to RMS res allocation.
 * result (RETURN)      NULL or a list of hostnames
 */
static hostlist_t _rms_wcoll(void)
{
    char *rhs;
    hostlist_t result = NULL;

    /* extract partition and resource ID from environment, if present */
    if ((rhs = getenv("RMS_RESOURCEID"))) {
        char *part, *ridstr = strchr(rhs, '.');
        int rid;

        if (!ridstr)
            errx("%p: malformed RMS_RESOURCEID value\n");
        *ridstr++ = '\0';
        rid = atoi(ridstr);
        part = rhs;

        result = _rms_rid_to_nodes(part, rid);
    }

    /*
     * Depend on PAM to keep user from setting RMS_RESOURCEID to
     * someone else's allocation and stealing cycles.  pam_rms should 
     * check to see if user has allocated the node before allowing qshd
     * authorization to succede.
     */

    return result;
}


/*
 * vi: tabstop=4 shiftwidth=4 expandtab
 */

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

#include <genders.h>

#include "src/common/hostlist.h"
#include "src/common/err.h"
#include "src/common/xmalloc.h"
#include "src/pdsh/mod.h"

#define ALL_NODES NULL

#ifndef GENDERS_ALTNAME_ATTRIBUTE
#  define GENDERS_ALTNAME_ATTRIBUTE   "altname"
#endif

#if STATIC_MODULES
#  define pdsh_module_info genders_module_info
#endif    

static hostlist_t genders_wcoll(opt_t *pdsh_opts);
static void       genders_fini(void);

static int genders_process_opt(opt_t *, int, char *);
static hostlist_t _read_genders(char *attr, int iopt);

#if !GENDERS_G_ONLY
static bool allnodes   = false;
static bool altnames   = false;
#endif /* !GENDERS_G_ONLY */
static char *gend_attr = NULL;

/* 
 * Export pdsh module operations structure
 */
struct pdsh_module_operations genders_module_ops = {
    (ModInitF)       NULL, 
    (ModExitF)       genders_fini, 
    (ModReadWcollF)  genders_wcoll, 
    (ModPostOpF)     NULL,
};

/* 
 * Export rcmd module operations
 */
struct pdsh_rcmd_operations genders_rcmd_ops = {
    (RcmdInitF)  NULL,
    (RcmdSigF)   NULL,
    (RcmdF)      NULL,
};

/* 
 * Export module options
 */
struct pdsh_module_option genders_module_options[] = 
 { 
   { 'g', "attribute", "target nodes with specified genders attribute",
     DSH | PCP, (optFunc) genders_process_opt 
   },
#if !GENDERS_G_ONLY
   { 'i', NULL,        "request canonical hostnames if applicable",
     DSH | PCP, (optFunc) genders_process_opt
   },
   { 'a', NULL,        "target all nodes", 
     DSH | PCP, (optFunc) genders_process_opt 
   },
#endif /* !GENDERS_G_ONLY */
   PDSH_OPT_TABLE_END
 };

/* 
 * Genders module info 
 */
struct pdsh_module pdsh_module_info = {
    "misc",
#if GENDERS_G_ONLY
    "genders-g",
#else
    "genders",
#endif /* GENDERS_G_ONLY */
    "Jim Garlick <garlick@llnl.gov>",
    "target nodes using libgenders and genders attributes",
    DSH | PCP, 

    &genders_module_ops,
    &genders_rcmd_ops,
    &genders_module_options[0],
};

#if 0
static int mod_genders_init(void)
{
    err("genders module initializing\n");
    return 0;
}
#endif

static int
genders_process_opt(opt_t *pdsh_opts, int opt, char *arg)
{
    switch (opt) {
#if !GENDERS_G_ONLY
    case 'a': 
        allnodes = true;
        break;
    case 'i':
        altnames = true;
        break;
#endif /* !GENDERS_G_ONLY */
    case 'g':
        gend_attr = Strdup(arg);
        break;
    default:
        err("%p: genders_process_opt: invalid option `%c'\n", opt);
        return -1;
        break;
    }
    return 0;
}

static void
genders_fini(void)
{
    if (gend_attr)
        Free((void **)&gend_attr);
    return;
}

/* 
 * Verify options passed to this module
 */
static void
_genders_opt_verify(opt_t *opt)
{
#if !GENDERS_G_ONLY
    if (altnames && !allnodes && (gend_attr == NULL)) {
        err("%p: Warning: Ignoring -i without -a or -g\n");
        altnames = false;
    }

    if (allnodes && (gend_attr != NULL))
        errx("%p: Do not specify -a with -g\n");
#endif /* !GENDERS_G_ONLY */

    if(opt->wcoll) {
#if !GENDERS_G_ONLY
        if (allnodes)
            errx("%p: Do not specify -a with other node selection options\n");
#endif /* !GENDERS_G_ONLY */
        if (gend_attr)
            errx("%p: Do not specify -g with other node selection options\n");
    }
}

static hostlist_t 
genders_wcoll(opt_t *opt)
{
    _genders_opt_verify(opt);

#if GENDERS_G_ONLY
    if (!gend_attr)
        return NULL;

    /* GENDERS_G_ONLY, altnamesnot anissue, always
     *use genders nodenames.
     */
    return _read_genders(gend_attr, true);
#else
    if (!allnodes && !altnames && !gend_attr)
        return NULL;

    if (allnodes)  
        gend_attr = ALL_NODES;

    return _read_genders(gend_attr, altnames);
#endif /* !GENDERS_G_ONLY */
}


static hostlist_t
_genders_to_altnames(genders_t g, char **nodes, int nnodes)
{
    hostlist_t hl = NULL;
    int maxvallen = 0;
    char *altname = NULL;
    char *altattr = GENDERS_ALTNAME_ATTRIBUTE;
    char *host    = NULL;
    int i, rc;

    if ((hl = hostlist_create(NULL)) == NULL)
        errx("%p: genders: hostlist_create: %m\n");

    if ((maxvallen = genders_getmaxvallen(g)) < 0)
        errx("%p: genders: getmaxvallen: %s\n", genders_errormsg(g));

    altname = Malloc(maxvallen + 1);

    for (i = 0; i < nnodes; i++) {
        memset(altname, '\0', maxvallen + 1);

        rc = genders_testattr(g, nodes[i], altattr, altname, maxvallen + 1);
        if (rc < 0) 
            errx("%p: genders: testattr failed: %s\n", genders_errormsg(g));

        host = rc == 0 ? nodes[i] : altname;
        if (hostlist_push_host(hl, host) <= 0)
            err("%p: genders: warning: target `%s' not parsed: %m", host);

    }

    Free((void **) &altname);

    return hl;
}


static hostlist_t 
_genders_to_hostlist(genders_t gh, char **nodes, int nnodes)
{
    hostlist_t hl = NULL;
    int i;

    if ((hl = hostlist_create(NULL)) == NULL)
        errx("%p: genders: hostlist_create failed: %m");

    for (i = 0; i < nnodes; i++) {
        if (hostlist_push_host(hl, nodes[i]) <= 0)
            err("%p: warning: target `%s' not parsed: %m\n", nodes[i]);
    }

    return hl;

}

static genders_t 
_handle_create()
{
    genders_t gh = NULL;

    if ((gh = genders_handle_create()) == NULL)
        errx("%p: Unable to create genders handle: %m\n");

    /* assumes genders file in default location */
    if (genders_load_data(gh, NULL) < 0)
        errx("%p: Unable to open genders file: %s\n", genders_errormsg(gh));

    return gh;
}

/*
 *  Search attr argument for an '=' char indicating an
 *   attr=value pair. If found, nullify '=' and return
 *   pointer to value part.
 *
 *  Returns NULL if no '=' found.
 */
static char *
_get_val(char *attr)
{
    char *val = NULL;

    if (attr == NULL)
        return (NULL);

    if ((val = strchr(attr, '='))) {
        *val = '\0';
        val++;
    }

    return (val);
}


static hostlist_t 
_read_genders(char *attr, int iopt)
{
    hostlist_t hl = NULL;
    genders_t  gh = NULL;
    char *val;
    char **nodes;
    int len, nnodes;

    val = _get_val(attr);

    gh = _handle_create();

    if ((len = genders_nodelist_create(gh, &nodes)) < 0)
        errx("%p: genders: nodelist_create: %s", genders_errormsg(gh));

    if ((nnodes = genders_getnodes(gh, nodes, len, attr, val)) < 0) {
        errx("%p: Error querying genders for attr \"%s\": %s\n", 
                attr ?: "(all)", genders_errormsg(gh));
    }

    if (!iopt)
        hl = _genders_to_altnames(gh, nodes, nnodes);
    else
        hl = _genders_to_hostlist(gh, nodes, nnodes);

    if (genders_nodelist_destroy(gh, nodes) < 0) {
        errx("%p: Error destroying genders node list: %s\n",
                genders_errormsg(gh));
    }

    if (genders_handle_destroy(gh) < 0) {
        errx("%p: Error destroying genders handle: %s\n",
                genders_errormsg(gh));
    }

    return hl;
}

/*
 * vi: tabstop=4 shiftwidth=4 expandtab
 */

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
#  define pdsh_module_priority genders_module_priority
#endif    

int pdsh_module_priority = DEFAULT_MODULE_PRIORITY;


/*
 *  Static genders module interface routines:
 */
static hostlist_t genders_wcoll(opt_t *pdsh_opts);
static int        genders_process_opt(opt_t *, int, char *);
static void       genders_fini(void);
static int        genders_postop(opt_t *);


#if !GENDERS_G_ONLY

static bool allnodes   = false;
static bool opt_i      = false;
#endif /* !GENDERS_G_ONLY */

static genders_t gh    = NULL;
static char *gend_attr = NULL;
static char *excl_attr = NULL;

/* 
 * Export pdsh module operations structure
 */
struct pdsh_module_operations genders_module_ops = {
    (ModInitF)       NULL, 
    (ModExitF)       genders_fini, 
    (ModReadWcollF)  genders_wcoll, 
    (ModPostOpF)     genders_postop,
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
   { 'X', "attribute", "exclude nodes with specified genders attribute",
     DSH | PCP, (optFunc) genders_process_opt
   },
#if !GENDERS_G_ONLY
   { 'i', NULL,        "request alternate or canonical hostnames if applicable",
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

/*
 * Static prototypes
 */
static genders_t  _handle_create();
static hostlist_t _genders_to_altnames(genders_t g, hostlist_t hl);
static hostlist_t _read_genders(char *attr);
static void       _genders_opt_verify(opt_t *opt);
static int        _delete_all (hostlist_t hl, hostlist_t dl);


/*
 *  Functions:
 */

genders_process_opt(opt_t *pdsh_opts, int opt, char *arg)
{
    switch (opt) {
#if !GENDERS_G_ONLY
    case 'a': 
        allnodes = true;
        break;
    case 'i':
        opt_i    = true;
        break;
#endif /* !GENDERS_G_ONLY */
    case 'g':
        gend_attr = Strdup(arg);
        break;
    case 'X':
        excl_attr = Strdup(arg);
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

    if (excl_attr)
        Free((void **)&excl_attr);

    if ((gh != NULL) && (genders_handle_destroy(gh) < 0))
        errx("%p: Error destroying genders handle: %s\n", genders_errormsg(gh));

    return;
}

static hostlist_t 
genders_wcoll(opt_t *opt)
{
    _genders_opt_verify(opt);

#if GENDERS_G_ONLY
    if (!gend_attr)
        return NULL;
#else
    if (!allnodes && !gend_attr)
        return NULL;

    if (allnodes)  
        gend_attr = ALL_NODES;

#endif /* !GENDERS_G_ONLY */

    if (gh == NULL)
        gh = _handle_create();

    return _read_genders(gend_attr);
}

static int
genders_postop(opt_t *opt)
{
    hostlist_t hl = NULL;

    if (!opt->wcoll)
        return (0);

    if (gh == NULL)
        gh = _handle_create();

    if (excl_attr && (hl = _read_genders (excl_attr))) {
        hostlist_t altlist = _genders_to_altnames (gh, hl);
        _delete_all (opt->wcoll, hl);
        _delete_all (opt->wcoll, altlist);

        hostlist_destroy (altlist);
        hostlist_destroy (hl);
    }

#if !GENDERS_G_ONLY
    /*
     *  Grab altnames if gend_attr or allnodes given and !opt_i,
     *   or if opt_i and neither gend_attr or allnodes given.
     */
    if (  ( opt_i && !(gend_attr || allnodes))
       || (!opt_i &&  (gend_attr || allnodes)) ) {
        hostlist_t hl = opt->wcoll;
        opt->wcoll = _genders_to_altnames(gh, hl);
        hostlist_destroy(hl);
    }
#endif
    return (0);
}


/* 
 * Verify options passed to this module
 */
static void
_genders_opt_verify(opt_t *opt)
{
#if !GENDERS_G_ONLY
/*    if (altnames && !allnodes && (gend_attr == NULL)) {
 *       err("%p: Warning: Ignoring -i without -a or -g\n");
 *       altnames = false;
 *   }
 */
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

    return;
}

static int
_maxnamelen (genders_t g)
{
    int maxvallen, maxnodelen;
    if ((maxvallen = genders_getmaxvallen(g)) < 0)
        errx("%p: genders: getmaxvallen: %s\n", genders_errormsg(g));
    if ((maxnodelen = genders_getmaxvallen(g)) < 0)
        errx("%p: genders: getmaxnodelen: %s\n", genders_errormsg(g));

    return (maxvallen > maxnodelen ? maxvallen : maxnodelen);
}


static hostlist_t
_genders_to_altnames(genders_t g, hostlist_t hl)
{
    hostlist_t retlist = NULL;
    hostlist_iterator_t i = NULL;
    int  maxlen = 0;
    char *altname = NULL;
    char *altattr = GENDERS_ALTNAME_ATTRIBUTE;
    char *host    = NULL;
    int  rc;

    if ((retlist = hostlist_create(NULL)) == NULL)
        errx("%p: genders: hostlist_create: %m\n");

    maxlen = _maxnamelen (g);
    altname = Malloc (maxlen + 1);

    if ((i = hostlist_iterator_create(hl)) == NULL)
        errx("%p: genders: hostlist_iterator_create: %m");

    while ((host = hostlist_next (i))) {
        memset(altname, '\0', maxlen);

        rc = genders_testattr(g, host, altattr, altname, maxlen + 1);

        /*
         *  If node not found, attempt to lookup canonical name via
         *   altername name.
         */
        if ((rc < 0) && (genders_errnum(g) == GENDERS_ERR_NOTFOUND)) 
            rc = genders_getnodes (g, &altname, 1, altattr, host);

        if (hostlist_push_host(retlist, (rc > 0 ? altname : host)) <= 0)
            err("%p: genders: warning: target `%s' not parsed: %m", host);

        free(host);
    }

    hostlist_iterator_destroy(i);

    Free((void **) &altname);

    return (retlist);
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

static genders_t _handle_create()
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
_read_genders(char *attr)
{
    hostlist_t hl = NULL;
    char *val;
    char **nodes;
    int len, nnodes;

    val = _get_val(attr);

    if ((len = genders_nodelist_create(gh, &nodes)) < 0)
        errx("%p: genders: nodelist_create: %s\n", genders_errormsg(gh));

    if ((nnodes = genders_getnodes(gh, nodes, len, attr, val)) < 0) {
        errx("%p: Error querying genders for attr \"%s\": %s\n", 
                attr ?: "(all)", genders_errormsg(gh));
    }

    hl = _genders_to_hostlist(gh, nodes, nnodes);

    if (genders_nodelist_destroy(gh, nodes) < 0) {
        errx("%p: Error destroying genders node list: %s\n",
                genders_errormsg(gh));
    }


    return hl;
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

/*
 * vi: tabstop=4 shiftwidth=4 expandtab
 */

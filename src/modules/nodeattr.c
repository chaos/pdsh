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

#include "hostlist.h"
#include "err.h"
#include "xmalloc.h"
#include "mod.h"
#include "xpopen.h"
#include "xstring.h"

#define ALL_NODES "all"

#ifndef LINEBUFSIZE
#  define LINEBUFSIZE 2048
#endif

#if STATIC_MODULES
#  define pdsh_module_info nodeattr_module_info
#endif    

static hostlist_t nodeattr_wcoll(opt_t *pdsh_opts);

static int nodeattr_process_opt(opt_t *, int, char *);
static hostlist_t _read_genders(char *attr, int iopt);

/*
 *  nodeattr module options
 *   -a      select all nodes
 *   -i      select alternate hostnames from genders
 *   -g attr select all nodes with genders attribute "attr"
 *
 */
#if !GENDERS_G_ONLY
static bool allnodes   = false;
static bool altnames   = false;
#endif /* !GENDERS_G_ONLY */
static char *gend_attr = NULL;

/* 
 * Export pdsh module operations structure
 */
struct pdsh_module_operations nodeattr_module_ops = {
    (ModInitF)       NULL, 
    (ModExitF)       NULL, 
    (ModReadWcollF)  nodeattr_wcoll, 
    (ModPostOpF)     NULL
};

/* 
 * Export rcmd module operations
 */
struct pdsh_rcmd_operations nodeattr_rcmd_ops = {
    (RcmdInitF)  NULL,
    (RcmdSigF)   NULL,
    (RcmdF)      NULL,
};

/* 
 * Export module options
 */
struct pdsh_module_option nodeattr_module_options[] = 
 { { 'g', "attribute", "target nodes with specified genders attribute",
     (optFunc) nodeattr_process_opt 
   },
#if !GENDERS_G_ONLY
   { 'a', NULL,        "target all nodes", 
     (optFunc) nodeattr_process_opt 
   },
   { 'i', NULL,        "request canonical hostnames if applicable",
     (optFunc) nodeattr_process_opt
   },
#endif /* !GENDERS_G_ONLY */
   PDSH_OPT_TABLE_END
 };

/* 
 * Nodeattr module info 
 */
struct pdsh_module pdsh_module_info = {
  "misc",
#if GENDERS_G_ONLY
  "nodeattr-g",
#else
  "nodeattr",
#endif /* GENDERS_G_ONLY */
  "Jim Garlick <garlick@llnl.gov>",
#if GENDERS_G_ONLY
  "target nodes using \"nodeattr -g attr\"",
#else
  "target nodes using \"nodeattr -[ai] [-g attr]\"",
#endif /* GENDERS_G_ONLY */
  DSH | PCP, 

  &nodeattr_module_ops,
  &nodeattr_rcmd_ops,
  &nodeattr_module_options[0],
};

static int
nodeattr_process_opt(opt_t *pdsh_opts, int opt, char *arg)
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


/* 
 * Verify options passed to this module
 */
static void
_nodeattr_opt_verify(opt_t *opt)
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
nodeattr_wcoll(opt_t *opt)
{
    _nodeattr_opt_verify(opt);

    if (!allnodes && !altnames && !gend_attr)
        return NULL;

    if (allnodes)  
        gend_attr = ALL_NODES;

    return _read_genders(gend_attr, altnames);
}


static hostlist_t 
_read_genders(char *attr, int iopt)
{
    FILE *f;
    hostlist_t hl = hostlist_create(NULL);
    char cmd[LINEBUFSIZE];
    char buf[LINEBUFSIZE];

    /* 
     *   xpopen sets uid back to real user id, so it is ok to use 
     *     "nodeattr" from user's path here
     */
    snprintf(cmd, sizeof(cmd), "%s -%sn %s", _PATH_NODEATTR, 
             iopt ? "" : "r", attr);

    f = xpopen(cmd, "r");
    if (f == NULL)
        errx("%p: error running \"%s\"\n", _PATH_NODEATTR);
    while (fgets(buf, LINEBUFSIZE, f) != NULL) {
        xstrcln(buf, NULL);
        if (hostlist_push_host(hl, buf) <= 0)
            err("%p: warning: target `%s' not parsed\n", buf);
    }
    if (xpclose(f) != 0)
        errx("%p: error running nodeattr\n");

    return hl;
}

/*
 * vi: tabstop=4 shiftwidth=4 expandtab
 */

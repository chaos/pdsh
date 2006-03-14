/*****************************************************************************\
 *  $Id$
 *****************************************************************************
 *  Copyright (C) 2005-2006 The Regents of the University of California.
 *  Produced at Lawrence Livermore National Laboratory (cf, DISCLAIMER).
 *  Written by Mark Grondona <mgrondona@llnl.gov>.
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
#  include <config.h>
#endif

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "src/common/err.h"
#include "src/common/xmalloc.h"
#include "src/common/xstring.h"
#include "src/common/list.h"
#include "opt.h"
#include "mod.h"
#include "rcmd.h"

/*
 *  Ordered preference for determining default rcmd method.
 *   Warning: If none of these modules are loaded, there will be no default.
 */
static char * rcmd_rank[] = 
    { "mrsh", "rsh", "ssh", "krb4", "qsh", "mqsh", "xcpu", NULL };

struct rcmd_module {
    char *       name;
    mod_t        mod;
    RcmdInitF    init;
    RcmdSigF     signal;
    RcmdF        rcmd;
    RcmdDestroyF rcmd_destroy;
};

struct node_rcmd_info {
    char *hostname;
    struct rcmd_module *rmod;
};

static List host_info_list = NULL;
static List rcmd_module_list = NULL;

static struct rcmd_module *default_rcmd_module = NULL;

static struct node_rcmd_info * 
node_rcmd_info_create (char *hostname, struct rcmd_module *module)
{
    struct node_rcmd_info *n = Malloc (sizeof (*n));

    if (!n)
        return NULL;

    n->hostname = Strdup (hostname);
    n->rmod     = module;

    return (n);
}

static void node_rcmd_info_destroy (struct node_rcmd_info *n)
{
    if (!n)
        return;

    Free ((void **)&n->hostname);
    Free ((void **)&n);
}

struct rcmd_module * rcmd_module_create (mod_t mod)
{
    struct rcmd_module *rmod = Malloc (sizeof (*rmod));

    rmod->mod  = mod;
    rmod->name = mod_get_name (mod);

    if (!(rmod->init = (RcmdInitF) mod_get_rcmd_init(mod))) {
        err("Unable to resolve \"rcmd_init\" in module \"%s\"\n",
                mod_get_name(mod));
        goto fail;
    }

    if (!(rmod->signal = (RcmdSigF) mod_get_rcmd_signal(mod))) {
        err("Unable to resolve \"rcmd_signal\" in module \"%s\"\n",
                mod_get_name(mod));
        goto fail;
    }

    if (!(rmod->rcmd = (RcmdF) mod_get_rcmd(mod))) {
        err("Unable to resolve \"rcmd\" in module \"%s\"\n",
                mod_get_name(mod));
        goto fail;
    }

    /*
     * Destroy function not required
     */
    rmod->rcmd_destroy = (RcmdDestroyF) mod_get_rcmd_destroy (mod);

    return (rmod);

  fail:
    Free ((void **) &rmod);
    return (NULL);
}

static void rcmd_module_destroy (struct rcmd_module *rmod)
{
    Free ((void **) &rmod);
}

static int find_rcmd_module (struct rcmd_module *x, char *name)
{
    return (strcmp (x->name, name) == 0);
}

static int find_host (struct node_rcmd_info *x, char *hostname)
{
    return (strcmp (x->hostname, hostname) == 0);
}

static struct rcmd_module * host_rcmd_module (char *host)
{
    struct node_rcmd_info *n = NULL;

    if ( host_info_list &&
         (n = list_find_first (host_info_list, (ListFindF) find_host, host)))
        return (n->rmod);

    return (default_rcmd_module);
}

static struct rcmd_module * rcmd_module_register (char *name)
{
    mod_t mod = NULL;
    struct rcmd_module *rmod = NULL;

    if (rcmd_module_list == NULL)
        rcmd_module_list = list_create ((ListDelF) rcmd_module_destroy);
    else
        rmod = list_find_first (rcmd_module_list, 
                                (ListFindF) find_rcmd_module, 
                                name);
    if (rmod != NULL)
        return (rmod);

    if (!(mod = mod_get_module ("rcmd", name))) {
        err ("No such rcmd module \"%s\"\n", name);
        return (NULL);
    }

    if (!(rmod = rcmd_module_create (mod)))
        return (NULL);

    if (!list_append (rcmd_module_list, rmod)) {
        err ("Failed to append rcmd module \"%s\"\n", name);
        rcmd_module_destroy (rmod);
        return (NULL);
    }

    return (rmod);
}

static int hostlist_register_rcmd (const char *hosts, struct rcmd_module *rmod)
{
    hostlist_t hl = hostlist_create (hosts);
    char * host;

    if (hl == NULL)
        return (-1);
    
    if (host_info_list == NULL)
        host_info_list = list_create ((ListDelF) node_rcmd_info_destroy);

    while ((host = hostlist_pop (hl))) {
        struct node_rcmd_info *n = NULL;

        /* 
         *  Do not override previously installed host info. First registered
         *   rcmd type for a host wins. This allows command line to override
         *   everything else.
         */
        if (list_find_first (host_info_list, (ListFindF) find_host, host))
            continue;

        if ((n = node_rcmd_info_create (host, rmod)) == NULL)
            errx ("Failed to create rcmd info for host \"%s\"\n", host);

        list_append (host_info_list, n);

        free (host);
    
    }

    hostlist_destroy (hl);

    return (0);
}


/*
 * Walk through list of default candidate modules, starting at head,
 *   and return the first module that is loaded.
 */
char * rcmd_get_default_module (void)
{
    mod_t mod = NULL;
    int i = 0;
    const char *name = NULL;

    while ((name = rcmd_rank[i++]) && !mod) 
        mod = mod_get_module ("rcmd", name);

    return mod ? mod_get_name (mod) : NULL;
}

int rcmd_register_default_module (char *hosts, char *rcmd_name)
{
    struct rcmd_module *rmod = NULL;

    if (rcmd_name == NULL)
        rcmd_name = rcmd_get_default_module ();

    if (!(rmod = rcmd_module_register (rcmd_name)))
        return (-1);

    /*  If host list is NULL, we are registering a new global default
     *   rcmd module. Set the convenience pointer and return
     */
    if (hosts == NULL) {
        default_rcmd_module = rmod;
        return (0);
    }

    if (hostlist_register_rcmd (hosts, rmod) < 0)
        return (-1);

    return (0);
}


struct rcmd_info * rcmd_info_create (int fd, int efd, struct rcmd_module *rmod,
                                     void *arg)
{
    struct rcmd_info *r = Malloc (sizeof (*r));

    if (r == NULL)
        return (NULL);

    r->fd = fd;
    r->efd = efd;
    r->rmod = rmod;
    r->arg = arg;

    return (r);
}

void rcmd_info_destroy (struct rcmd_info *r)
{
    Free ((void **) &r);
}


struct rcmd_info * rcmd_create (char *ahost, char *addr, char *locuser,
                                char *remuser, char *cmd, int nodeid, 
                                bool error_fd)
{
    int fd, efd = -1;
    void *arg;
    struct rcmd_info *rcmd = NULL;
    struct rcmd_module *rmod = NULL;

    if ((rmod = host_rcmd_module (ahost)) == NULL)
        return (NULL);

    fd = (*rmod->rcmd) (ahost, addr, locuser, remuser, cmd, nodeid, 
                        error_fd ? &efd : NULL, &arg);

    if (fd < 0)
        return (NULL);
    
    if ((rcmd = rcmd_info_create (fd, efd, rmod, arg)) == NULL) {
        err ("%p: Unable to create rcmd info for \"%s\"\n", ahost);
        (*rmod->rcmd_destroy) (arg);
        return (NULL);
    }

    return (rcmd);
}

int rcmd_destroy (struct rcmd_info *rcmd)
{
    int rc = 0;

    if (rcmd == NULL)
        return (0);
    if (rcmd->rmod->rcmd_destroy)
        rc = (*rcmd->rmod->rcmd_destroy) (rcmd->arg);
    rcmd_info_destroy (rcmd);

    return (rc);
}

int rcmd_signal (struct rcmd_info *rcmd, int signum)
{
    assert (rcmd != NULL);
    assert (rcmd->rmod != NULL);

    return (*rcmd->rmod->signal) (rcmd->efd, rcmd->arg, signum);
}

int rcmd_init (opt_t *opt)
{
    struct rcmd_module *r = NULL;
    ListIterator i;

    if (!rcmd_module_list) {
        (*default_rcmd_module->init) (opt);
        return (0);
    }

    i = list_iterator_create (rcmd_module_list);
    while ((r = list_next (i)))
        (*r->init) (opt);

    list_iterator_destroy (i);

    return (0);
}

int rcmd_exit (void)
{
    if (host_info_list)
        list_destroy (host_info_list);
    if (rcmd_module_list)
        list_destroy (rcmd_module_list);

    return (0);
}

/*
 * vi: ts=4 sw=4 expandtab
 */

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
#include <errno.h>

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
#if defined(RCMD_RANK_LIST)
    { RCMD_RANK_LIST, NULL };
#else
    { "mrsh", "rsh", "ssh", "krb4", "qsh", "mqsh", "exec", "xcpu", NULL };
#endif /* RCMD_RANK_LIST */

struct rcmd_module {
    char *              name;
    mod_t               mod;
    struct rcmd_options options;
    RcmdInitF           init;
    RcmdSigF            signal;
    RcmdF               rcmd;
    RcmdDestroyF        rcmd_destroy;
};

struct node_rcmd_info {
    char *hostname;
    char *username;
    struct rcmd_module *rmod;
};

static List host_info_list = NULL;
static List rcmd_module_list = NULL;

static struct rcmd_module *default_rcmd_module = NULL;
static struct rcmd_module *current_rcmd_module = NULL;

static struct node_rcmd_info *
node_rcmd_info_create (char *hostname, char *user, struct rcmd_module *module)
{
    struct node_rcmd_info *n = Malloc (sizeof (*n));

    if (!n)
        return NULL;

    n->hostname = Strdup (hostname);
    n->username = Strdup (user);
    n->rmod     = module;

    return (n);
}

static void node_rcmd_info_destroy (struct node_rcmd_info *n)
{
    if (!n)
        return;

    Free ((void **)&n->hostname);
    if (n->username)
        Free ((void **)&n->username);
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

    rmod->options.resolve_hosts = 1;

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

static struct node_rcmd_info * host_rcmd_info (char *host)
{
    if (host_info_list == NULL)
        return (NULL);

    return (list_find_first (host_info_list, (ListFindF) find_host, host));
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

static int hostlist_register_rcmd (const char *hosts, struct rcmd_module *rmod,
                                   char *user)
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

        if ((n = node_rcmd_info_create (host, user, rmod)) == NULL)
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
 *   Unless rcmd_default_module is already registered.
 */
char * rcmd_get_default_module (void)
{
    mod_t mod = NULL;
    int i = 0;
    const char *name = NULL;

    if (default_rcmd_module != NULL)
        return (default_rcmd_module->name);

    while ((name = rcmd_rank[i++]) && !mod)
        mod = mod_get_module ("rcmd", name);

    return mod ? mod_get_name (mod) : NULL;
}

int rcmd_register_default_rcmd (char *rcmd_name)
{
    struct rcmd_module *rmod = NULL;
    if (!(rmod = rcmd_module_register (rcmd_name)))
        return (-1);
    default_rcmd_module = rmod;
    return (0);
}

int rcmd_register_defaults (char *hosts, char *rcmd_name, char *user)
{
    struct rcmd_module *rmod = NULL;

    if (rcmd_name && !(rmod = rcmd_module_register (rcmd_name)))
        return (-1);

    /*  If host list is NULL, we are registering a new global default
     *   rcmd module. Set the convenience pointer and return
     */
    if (hosts == NULL) {
        default_rcmd_module = rmod;
        return (0);
    }

    if (hostlist_register_rcmd (hosts, rmod, user) < 0)
        return (-1);

    return (0);
}


struct rcmd_info * rcmd_info_create (struct rcmd_module *rmod)
{
    struct rcmd_info *r = Malloc (sizeof (*r));

    if (r == NULL)
        return (NULL);

    r->fd = -1;
    r->efd = -1;
    r->rmod = rmod;
    r->opts = &rmod->options;
    r->arg = NULL;
    r->ruser = NULL;

    return (r);
}

void rcmd_info_destroy (struct rcmd_info *r)
{
    Free ((void **) &r);
}

struct rcmd_info * rcmd_create (char *host)
{
    struct rcmd_info *rcmd = NULL;
    struct rcmd_module *rmod = NULL;
    struct node_rcmd_info *n = NULL;

    if ((n = host_rcmd_info (host))) {
        rmod = n->rmod;
    }

    /*
     * If no rcmd module use default
     */
    if (rmod == NULL) {
        if ((rmod = default_rcmd_module) == NULL) {
            err ("%p: No rcmd module for \"%s\"\n", host);
            return (NULL);
        }
    }

    if ((rcmd = rcmd_info_create (rmod)) == NULL) {
        err ("%p: Unable to allocate rcmd info for \"%s\"\n", host);
        return (NULL);
    }

    if (n != NULL && n->username)
        rcmd->ruser = n->username;

    return (rcmd);
}


int rcmd_connect (struct rcmd_info *rcmd, char *ahost, char *addr,
                  char *locuser, char *remuser, char *cmd, int nodeid,
                  bool error_fd)
{
    /*
     *  rcmd->ruser overrides default
     */
    if (rcmd->ruser)
        remuser = rcmd->ruser;

    rcmd->fd = (*rcmd->rmod->rcmd) (ahost, addr, locuser, remuser, cmd, nodeid,
                                    error_fd ? &rcmd->efd : NULL, &rcmd->arg);
    return (rcmd->fd);
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
        if (default_rcmd_module == NULL)
            return (-1);
        current_rcmd_module = default_rcmd_module;
        (*default_rcmd_module->init) (opt);
        current_rcmd_module = NULL;
        return (0);
    }

    i = list_iterator_create (rcmd_module_list);
    while ((r = list_next (i))) {
        current_rcmd_module = r;
        (*r->init) (opt);
        current_rcmd_module = NULL;
    }

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

int rcmd_opt_set (int id, void * value)
{
    if (current_rcmd_module == NULL) {
        errno = ESRCH;
        return (-1);
    }

    switch (id) {
        case RCMD_OPT_RESOLVE_HOSTS:
            current_rcmd_module->options.resolve_hosts = (long int) value;
            break;
        default:
            errno = EINVAL;
            return (-1);
    }

    return (0);
}

/*
 * vi: ts=4 sw=4 expandtab
 */

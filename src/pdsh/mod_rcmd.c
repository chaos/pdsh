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
#  include <config.h>
#endif

#include <assert.h>

#include "src/common/err.h"
#include "src/common/xmalloc.h"
#include "src/common/xstring.h"
#include "opt.h"
#include "mod.h"
#include "mod_rcmd.h"

/*
 * Ordered preference for determining default rcmd method
 *   Warning: If none of these are loaded, there will be no default.
 */
static char * rcmd_rank[] = 
    { "mrsh", "rsh", "ssh", "krb4", "qsh", "mqsh", NULL };

/* 
 *  Container for information specific to rcmd modules.
 */
struct _rmodule {
    RcmdInitF    init;
    RcmdSigF     signal;
    RcmdF        rcmd;
    RcmdDestroyF rcmd_destroy;
};

/*
 *  Only one rcmd module can be used at a time, so we define a
 *    single rmodule container.
 */
static struct _rmodule *rmod;

/*
 *  Called within opt.c to load requested rcmd module, or default if no
 *    module was specifically requested. Loads rcmd-specific symbols from
 *    module, if exported.  Each rcmd module must have an init, signal, and
 *    base function, even if the function does nothing.  
 *
 *  Returns 0 on success and -1 on failure.
 */
int 
mod_rcmd_load(opt_t * opt)
{
    mod_t mod = NULL;
    rmod = Malloc(sizeof(*rmod));

    if (!opt->rcmd_name) {
        opt->rcmd_name = Strdup(mod_rcmd_get_default_module());
        if (!opt->rcmd_name) {
            err("%p: Unable to find a default rcmd module.\n");
            goto fail;
        }
    }

    if (!(mod = mod_get_module("rcmd", opt->rcmd_name))) {
        err("%p: Unable to find rcmd module \"%s\"\n", opt->rcmd_name);
        goto fail;
    }

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

    return 0;

  fail:
    Free((void **) &rmod);
    return -1;
}


/*
 * Walk through list of default candidate modules, starting at head,
 *   and return the first module that is loaded.
 */
char * mod_rcmd_get_default_module(void)
{
    mod_t mod = NULL;
    int i = 0;
    const char *name = NULL;
    
    while ((name = rcmd_rank[i++]) && !mod) {
      mod = mod_get_module("rcmd", name);
    }

    return mod ? mod_get_name(mod) : NULL;
}


int
mod_rcmd_init(opt_t *opt)
{
    assert(rmod != NULL);
    return ((*rmod->init)(opt));
}


void
mod_rcmd_exit(void)
{
    Free((void **) &rmod);
}


int mod_rcmd_signal(struct rcmd_info *rcmd, int signum)
{
    assert(rmod != NULL);
    return (*rmod->signal) (rcmd->efd, rcmd->arg, signum);
}

struct rcmd_info *
mod_rcmd_create (char *ahost, char *addr, char *locuser, char *remuser, 
                 char *cmd, int nodeid, bool err)
{
    int fd, efd = -1;
    void *arg;
    struct rcmd_info *rcmd = NULL;

    assert(rmod != NULL);

    fd = (*rmod->rcmd) (ahost, addr, locuser, remuser, cmd, nodeid, 
                        err ? &efd : NULL, &arg);

    if (fd < 0)
        return NULL;

    rcmd = Malloc (sizeof (*rcmd));
    rcmd->fd = fd;
    rcmd->efd = efd;
    rcmd->arg = arg;

    return (rcmd);
}

int mod_rcmd_destroy (struct rcmd_info *rcmd)
{
    int rc = 0;

    if (rcmd == NULL)
        return (0);
    if (rmod->rcmd_destroy != NULL)
        rc = (*rmod->rcmd_destroy) (rcmd->arg);
    Free ((void **) &rcmd);

    return (rc);
}


/* 
 * vi:tabstop=4 shiftwidth=4 expandtab
 */


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

#include "err.h"
#include "xmalloc.h"
#include "xstring.h"
#include "opt.h"

#include "mod.h"
#include "mod_rcmd.h"

/*
 * Ordered preference for determining default rcmd method
 *   Warning: If none of these are loaded, there will be no default.
 */
static char * rcmd_rank[] = { "bsd", "ssh", "k4cmd", "qsh", 
#if HAVE_MRSH
                              "mrsh",
#endif
                              NULL };

typedef int (*RcmdInitF) (opt_t *);
typedef int (*RcmdSigF)  (int efd, int signum);
typedef int (*RcmdF)     (char *, char *, char *, char *, char *, int, int *);

/* 
 *  Container for information specific to rcmd modules.
 */
struct _rmodule {
	mod_t mod;
    RcmdInitF init;
    RcmdSigF  signal;
    RcmdF     rcmd;
};

/*
 *  Only one rcmd module can be used at a time, so we define a
 *    single rmodule container.
 */
static struct _rmodule *rmod;


/*
 *  Called within opt.c to load requested rcmd module, or default if no
 *    module was specifically requested. Loads rcmd-specific symbols from
 *    module, if exported. Symbols "pdsh_signal" and "pdsh_rcmd" *must*
 *    be exported from the module or the load will fail.
 *
 *  Returns 0 on success and -1 on failure.
 */
int 
mod_rcmd_load(opt_t * opt)
{
    rmod = Malloc(sizeof(*rmod));

    if (!opt->rcmd_name) {
        opt->rcmd_name = Strdup(mod_rcmd_get_default_module());
        if (!opt->rcmd_name) {
            err("%p: Unable to find a default rcmd module.\n");
            goto fail;
        }
    }

    if (!(rmod->mod = mod_get_module("rcmd", opt->rcmd_name))) {
        err("%p: Unable to find rcmd module \"%s\"\n", opt->rcmd_name);
        goto fail;
    }

	rmod->init = (RcmdInitF) mod_get_sym(rmod->mod, "pdsh_rcmd_init");

	if (!(rmod->signal = (RcmdSigF) mod_get_sym(rmod->mod, "pdsh_signal" ))) {
        err("Unable to resolve \"pdsh_signal\" in module \"%s\"\n", "XXX");
		goto fail;
    }
	if (!(rmod->rcmd = (RcmdF) mod_get_sym(rmod->mod, "pdsh_rcmd"   ))) {
        err("Unable to resolve \"pdsh_rcmd\" in module \"%s\"\n", "XXX");
		goto fail;
    }

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

    while ((name = rcmd_rank[i++]) && !mod) 
        mod = mod_get_module("rcmd", name);

   return mod ? mod_get_name(mod) : NULL;
}


int
mod_rcmd_init(opt_t *opt)
{
	if (rmod->init)
		return ((*rmod->init)(opt));
    else 
        return 0;
}


void
mod_rcmd_exit(void)
{
    Free((void **) &rmod);
}


int mod_rcmd_signal(int efd, int signum)
{
	assert(rmod != NULL);

    return (*rmod->signal) (efd, signum);
}

int
mod_rcmd(char *ahost, char *addr, char *locuser, char *remuser, char *cmd,
     int nodeid, int *fd2p)
{
	assert(rmod != NULL);
    return (*rmod->rcmd) (ahost, addr, locuser, remuser, cmd, nodeid, fd2p);
}


/* 
 * vi:tabstop=4 shiftwidth=4 expandtab
 */


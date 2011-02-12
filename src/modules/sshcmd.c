/*****************************************************************************\
 *  $Id$
 *****************************************************************************
 *  Copyright (C) 2001-2006 The Regents of the University of California.
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

/*
 * This is an rcmd() replacement originally by Chris Siebenmann 
 * <cks@utcc.utoronto.ca>.  There was no copyright information on the original.
 * If this finds its way back to the original author please let me know if
 * you would like this header block changed...
 * 
 * Brought in to pdsh from USC rdist -jg
 * Changes:
 * - added fd2p arg handling
 * - changed name, func prototype, and added sshcmd() and rshcmd() wrappers
 * - use 'err' for output
 * - unset DISPLAY and call setsid() so ssh won't hang prompting for passphrase
 */

#if     HAVE_CONFIG_H
#include "config.h"
#endif


#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <stdlib.h>             /* putenv */
#if	HAVE_UNISTD_H
#include <unistd.h>
#endif
#if	HAVE_PTHREAD_H
#include <pthread.h>
#endif
#include <string.h>             /* memset */

#include <stddef.h>
#include <sys/socket.h> 
#include <sys/wait.h>

#include "src/common/xmalloc.h"
#include "src/common/xstring.h"
#include "src/common/err.h"
#include "src/common/list.h"
#include "src/common/split.h"
#include "src/common/pipecmd.h"
#include "src/pdsh/dsh.h"
#include "src/pdsh/mod.h"

#define HBUF_LEN	1024

#if STATIC_MODULES
#  define pdsh_module_info sshcmd_module_info
#  define pdsh_module_priority sshcmd_module_priority
#endif    

int pdsh_module_priority = DEFAULT_MODULE_PRIORITY;

    
static int mod_ssh_postop(opt_t *opt);
static int mod_ssh_exit (void);

static int sshcmd_init(opt_t *);
static int sshcmd_signal(int, void *arg, int);
static int sshcmd(char *, char *, char *, char *, char *, int, int *, void **);
static int sshcmd_destroy (pipecmd_t p);
static int sshcmd_args_init (void);

List ssh_args_list =     NULL;

/*
 *  Export generic pdsh module operations:
 */
struct pdsh_module_operations sshcmd_module_ops = {
    (ModInitF)       NULL,
    (ModExitF)       mod_ssh_exit,
    (ModReadWcollF)  NULL,
    (ModPostOpF)     mod_ssh_postop
};

/*
 *  Export rcmd module operations
 */
struct pdsh_rcmd_operations sshcmd_rcmd_ops = {
    (RcmdInitF)    sshcmd_init,
    (RcmdSigF)     sshcmd_signal,
    (RcmdF)        sshcmd,
    (RcmdDestroyF) sshcmd_destroy
};

/* 
 * Export module options
 */
struct pdsh_module_option sshcmd_module_options[] = 
 { 
   PDSH_OPT_TABLE_END
 };

/* 
 * Sshcmd module info 
 */
struct pdsh_module pdsh_module_info = {
  "rcmd",
  "ssh",
  "Jim Garlick <garlick@llnl.gov>",
  "ssh based rcmd connect method",
  DSH | PCP, 

  &sshcmd_module_ops,
  &sshcmd_rcmd_ops,
  &sshcmd_module_options[0],
};

static char **ssh_argv_create (const char **remote_argv)
{
    int n;
    char *arg;
    char **argv;
    const char **p;
    ListIterator i;

    n = 0;
    for (p = remote_argv; *p; p++)
        n++;

    n += list_count (ssh_args_list) + 2;
    argv = (char **) Malloc (n * sizeof (char *));
    memset (argv, 0, n);

    n = 0;
    i = list_iterator_create (ssh_args_list);
    while ((arg = list_next (i))) 
        argv[n++] = Strdup (arg);
    list_iterator_destroy (i);

    /* Append remote_argv to standard list of args */
    for (p = remote_argv; *p; p++)
        argv[n++] = Strdup (*p);

    return (argv);
}

static void ssh_argv_destroy (char **args)
{
    int i = 0;
    while (args[i])
        Free ((void **) &args[i++]);
    Free ((void **) &args);
}

static int ssh_args_prepend_timeout (int timeout)
{
#if SSH_HAS_CONNECT_TIMEOUT
    char buf[64];

    if (timeout <= 0)
        return (0);

    snprintf (buf, 64, SSH_CONNECT_TIMEOUT_OPTION, timeout);
    list_prepend (ssh_args_list, Strdup (buf));
#endif
    return (0);
}

static int mod_ssh_postop(opt_t *opt)
{
    sshcmd_args_init ();
    ssh_args_prepend_timeout (opt->connect_timeout);

    /*
     *  Append PATH=...; to ssh args if DSHPATH was set
     */
    if (opt->dshpath)
        list_append (ssh_args_list, Strdup (opt->dshpath));

    return 0;
}

static int sshcmd_init(opt_t * opt)
{
    /*
     * Drop privileges if running setuid root
     */
    if ((geteuid() == 0) && (getuid() != 0))
        setuid (getuid ());

    /*
     *  Do not resolve hostnames in pdsh when using ssh
     */
    if (rcmd_opt_set (RCMD_OPT_RESOLVE_HOSTS, 0) < 0)
        errx ("%p: sshcmd_init: rcmd_opt_set: %m\n");

    return 0;
}

static int mod_ssh_exit (void)
{
    if (ssh_args_list)
        list_destroy (ssh_args_list);

    return 0;
}

/*
 * SSH doesn't support signal forwarding, at least the way pdsh uses it
 *  at this time. Instead we always send SIGTERM which seems to have the
 *  desired effect of killing off ssh most of the time.
 */
static int sshcmd_signal(int fd, void *arg, int signum)
{
    /*
     *  Always send SIGTERM. SIGINT doesn't seem to get forwarded by ssh, and
     *    really termination of the connection is probably the desired result.
     */
    err ("sending SIGTERM to ssh %s\n", pipecmd_target ((pipecmd_t) arg));
    return (pipecmd_signal ((pipecmd_t) arg, SIGTERM));
}

static int
sshcmd(char *ahost, char *addr, char *luser, char *ruser, char *cmd,
         int rank, int *fd2p, void **arg)
{
    pipecmd_t p = NULL;
    char **ssh_args = ssh_argv_create (pdsh_remote_argv ());

    if (!(p = pipecmd ("ssh", (const char **) ssh_args, ahost, ruser, rank)))
        goto out;

    if (fd2p)
        *fd2p = pipecmd_stderrfd (p);

    *arg = (void *) p;

out:
    ssh_argv_destroy (ssh_args);
    return (p ? pipecmd_stdoutfd (p) : -1);
}

static int 
sshcmd_destroy (pipecmd_t p)
{
    int status = 0;

    if (pipecmd_wait (p, &status) < 0)
        err ("%p: %S: wait on ssh cmd: %m\n", pipecmd_target (p));  

    pipecmd_destroy (p);

    return WEXITSTATUS (status);
}


static int sshcmd_args_init (void)
{
    char *val = NULL;
    char *str = NULL;
    List args_list = NULL;

    if (!(val = getenv ("PDSH_SSH_ARGS"))) 
        val = "-2 -a -x -l%u %h";

    str = Strdup (val);
    args_list = list_split (" ", str);
    Free ((void **) &str);

    if ((val = getenv ("PDSH_SSH_ARGS_APPEND"))) {
        str = Strdup (val);
        args_list = list_split_append (args_list, " ", str);
        Free ((void **) &str);
    }

    ssh_args_list = args_list;

    return (0);
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

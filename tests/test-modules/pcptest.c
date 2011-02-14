/*****************************************************************************\
 *  $Id$
 *****************************************************************************
 *  Copyright (C) 2007-2011 Lawrence Livermore National Security, LLC.
 *  Copyright (C) 2001-2006 The Regents of the University of California.
 *  Produced at Lawrence Livermore National Laboratory (cf, DISCLAIMER).
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
 *  This module uses the "pipecmd" interface to execute
 *   a process in a subdirectory based on the remote hostname.
 *   Used for pdcp/rpdcp testing.
 *
 */

#if     HAVE_CONFIG_H
#include "config.h"
#endif


#include <sys/wait.h>
#include <string.h>
#include <unistd.h>

#include "src/pdsh/opt.h"
#include "src/pdsh/mod.h"
#include "src/pdsh/rcmd.h"
#include "src/common/pipecmd.h"
#include "src/common/err.h"
#include "src/common/xmalloc.h"
#include "src/common/xstring.h"

int pdsh_module_priority = DEFAULT_MODULE_PRIORITY;

static int mod_pcptest_postop(opt_t *opt);
static int mod_pcptest_exit (void);

static int pcptest_init(opt_t *);
static int pcptest_signal(int, void *arg, int);
static int pcptest(char *, char *, char *, char *, char *, int, int *, void **);
static int pcptest_destroy (pipecmd_t p);

/*
 *  Export generic pdsh module operations:
 */
struct pdsh_module_operations pcptest_module_ops = {
    (ModInitF)       NULL,
    (ModExitF)       mod_pcptest_exit,
    (ModReadWcollF)  NULL,
    (ModPostOpF)     mod_pcptest_postop
};

/*
 *  Export rcmd module operations
 */
struct pdsh_rcmd_operations pcptest_rcmd_ops = {
    (RcmdInitF)    pcptest_init,
    (RcmdSigF)     pcptest_signal,
    (RcmdF)        pcptest,
    (RcmdDestroyF) pcptest_destroy
};

/*
 * Export module options
 */
struct pdsh_module_option pcptest_module_options[] =
 {
   PDSH_OPT_TABLE_END
 };

/*
 * Sshcmd module info
 */
struct pdsh_module pdsh_module_info = {
  "rcmd",
  "pcptest",
  "Mark Grondona <mgrondona@llnl.gov>",
  "Rcmd connect module used for pdcp/rpdcp testing",
  PCP,
  &pcptest_module_ops,
  &pcptest_rcmd_ops,
  &pcptest_module_options[0],
};


static int mod_pcptest_postop(opt_t *opt)
{
    if (opt->rcmd_name && strcmp(opt->rcmd_name, "pcptest") == 0) {
        if (opt->connect_timeout != CONNECT_TIMEOUT) {
            err("%p: Cannot specify -t with \"-R pcptest\"\n");
            return 1;
        }
    }
    return 0;
}
static int pcptest_init(opt_t * opt)
{
    /*
     * Drop privileges if running setuid root
     */
    if ((geteuid() == 0) && (getuid() != 0))
        setuid (getuid ());

    /*
     *  Do not resolve hostnames in pdsh when using pcptest
     */
    if (rcmd_opt_set (RCMD_OPT_RESOLVE_HOSTS, 0) < 0)
        errx ("%p: pcptest_init: rcmd_opt_set: %m\n");

    return 0;
}

static int mod_pcptest_exit (void)
{
    return 0;
}

static int pcptest_signal(int fd, void *arg, int signum)
{
    return (pipecmd_signal ((pipecmd_t) arg, signum));
}

static char **pcptest_argv_create (char *remote_cmd)
{
    char *cmd;
    char **argv;
    const char **p;

    /*  Prepend chdir to remote argv, then collapse args
     *   so they can be fed to /bin/sh -c
     */
    cmd = Strdup ("cd %h; ");
    xstrcat (&cmd, remote_cmd);

    argv = Malloc (4 * sizeof (char *));
    argv[0] = Strdup ("/bin/sh");
    argv[1] = Strdup ("-c");
    argv[2] = cmd;
    argv[3] = NULL;

    return argv;
}

static int
pcptest(char *ahost, char *addr, char *luser, char *ruser, char *cmd,
       int rank, int *fd2p, void **arg)
{
    pipecmd_t p;
    const char **argv = pcptest_argv_create (cmd);

    if (!(p = pipecmd (argv[0], argv + 1,  ahost, ruser, rank)))
        return (-1);

    if (fd2p)
        *fd2p = pipecmd_stderrfd (p);

    *arg = p;

    return (pipecmd_stdoutfd (p));
}

static int
pcptest_destroy (pipecmd_t p)
{
    int status;

    if (pipecmd_wait (p, &status) < 0)
        return (1);

    pipecmd_destroy (p);

    return (WEXITSTATUS (status));
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

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
#include "src/pdsh/dsh.h"
#include "src/pdsh/mod.h"

#define HBUF_LEN	1024

#if STATIC_MODULES
#  define pdsh_module_info sshcmd_module_info
#  define pdsh_module_priority sshcmd_module_priority
#endif    

int pdsh_module_priority = DEFAULT_MODULE_PRIORITY;

struct ssh_info_struct {
    pid_t ssh_pid;          /* PID of ssh command         */
    char *target;           /* Hostname of ssh target     */
    int fd;                 /* stderr fd to ssh command for signals  */
};

void ssh_info_destroy (struct ssh_info_struct *s);
struct ssh_info_struct * ssh_info_create (pid_t pid, const char *host, int fd);
    
static int mod_ssh_postop(opt_t *opt);
static int mod_ssh_exit (void);

static int sshcmd_init(opt_t *);
static int sshcmd_signal(int, void *arg, int);
static int sshcmd(char *, char *, char *, char *, char *, int, int *, void **);
static int sshcmd_destroy (struct ssh_info_struct *s);
static int sshcmd_args_init (void);
static char **ssh_args_create (char *host, char *user, char *cmd);
static void ssh_args_destroy (char **);


static char **ssh_args            = NULL;
static int    ssh_args_len        = -1;
static int    ssh_args_host_index = -1;


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

static int mod_ssh_postop(opt_t *opt)
{
    if (strcmp(opt->rcmd_name, "ssh") == 0) {
        if (opt->connect_timeout != CONNECT_TIMEOUT) {
            err("%p: Cannot specify -t with \"-R ssh\"\n");
            return 1;
        }

        /*
         *  Don't need hostname resolution for ssh
         */
        opt->resolve_hosts = false;

        /*
         * Set the signal mask for the "main" thread to block
         *  SIGCHLD so the ssh_reaper can collect exit codes 
         */
    }
    return 0;
}
static int sshcmd_init(opt_t * opt)
{
    /*
     * Drop privileges if running setuid root
     */
    if ((geteuid() == 0) && (getuid() != 0))
        setuid (getuid ());

    sshcmd_args_init ();

    return 0;
}

static int mod_ssh_exit (void)
{
    if (ssh_args) {
        int i;
        for (i = 0; i < ssh_args_len; i++) {
            if (ssh_args[i])
                Free ((void **) &ssh_args[i]);
        }
        Free ((void **) &ssh_args);
    }

    return 0;
}

/*
 * SSH doesn't support signal forwarding, at least the way pdsh uses it
 *  at this time. Instead we always send SIGTERM which seems to have the
 *  desired effect of killing off ssh most of the time.
 */
static int sshcmd_signal(int fd, void *arg, int signum)
{
    struct ssh_info_struct *s = arg;
    if (s == NULL)
        return -1;

    /*
     *  Always send SIGTERM. SIGINT doesn't seem to get forwarded by ssh, and
     *    really termination of the connection is probably the desired result.
     */
    err ("sending SIGTERM to ssh %s pid %d\n", s->target, (int) s->ssh_pid);
    return (kill (s->ssh_pid, SIGTERM));
}

static void
closeall (int fd)
{
    int fdlimit = sysconf (_SC_OPEN_MAX);

    while (fd < fdlimit)
        close (fd++);

    return;
}

/*
 * This is a replacement rcmd() function that uses an arbitrary
 * program in place of a direct rcmd() function call.
 */
static int _pipecmd(char *path, char *args[], const char *ahost, int *fd2p,
                    struct ssh_info_struct **spp)
{
    int cpid;
    int sp[2], esp[2];

    /* get a socketpair we'll use for stdin and stdout. */
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sp) < 0) {
        err("%p: socketpair failed for %S: %m\n", ahost);
        return -1;
    }
    if (fd2p) {
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, esp) < 0) {
            err("%p: socketpair failed for %S: %m\n", ahost);
            return -1;
        }
    }

    cpid = fork();
    if (cpid < 0) {
        err("%p: fork failed for %S: %m\n", ahost);
        return -1;
    }
    if (cpid == 0) {
        /* child. we use sp[1] to be stdin/stdout, and close
           sp[0]. */
        (void) close(sp[0]);
        if (dup2(sp[1], 0) < 0 || dup2(0, 1) < 0) {
            err("%p: dup2 failed for %S: %m\n", ahost);
            _exit(255);
        }
        if (fd2p) {             /* stderr has own socketpair */
            close(esp[0]);
            if (dup2(esp[1], 2) < 0) {
                err("%p: dup2 failed for %S: %m\n", ahost);
                _exit(255);
            }
        } else {                /* stderr goes with stdout, stdin on sp[1] */
            if (dup2(0, 2) < 0) {
                err("%p: dup2 failed for %S: %m\n", ahost);
                _exit(255);
            }
        }

        /*
         * Try to close all stray file descriptors before invoking ssh
         *  to ensure that ssh stdin is closed when pdcp/pdsh close their
         *  end of the socketpair.
         */
        closeall (3);

        setsid();
        putenv("DISPLAY=");
        execvp(path, args);
        err("%p: execlp %s failed for %S: %m.", path, ahost);
        _exit(255);
    }
    if (cpid > 0) {
        /* parent. close sp[1], return sp[0]. */
        (void) close(sp[1]);

        if (fd2p) {
            close(esp[1]);
            *fd2p = esp[0];
        }

        *spp = ssh_info_create (cpid, ahost, esp[0]);

        return sp[0];
    }
    /*NOTREACHED*/ return 0;
}

static int
sshcmdrw(char *ahost, char *addr, char *luser, char *ruser, char *cmd,
       int rank, int *fd2p, struct ssh_info_struct **s)
{
    char **args = ssh_args_create (ahost, ruser, cmd);

    int rc = _pipecmd("ssh", args, ahost, fd2p, s);

    ssh_args_destroy (args);

    return (rc);
}

static int
sshcmd(char *ahost, char *addr, char *luser, char *ruser, char *cmd,
         int rank, int *fd2p, void **arg)
{
    struct ssh_info_struct *ssh_info = NULL;

    int rc = sshcmdrw(ahost, addr, luser, ruser, cmd, rank, fd2p, &ssh_info);

    *arg = (void *) ssh_info;

    return rc;
}

static int 
sshcmd_destroy (struct ssh_info_struct *s)
{
    int status = 0;

    if (s == NULL)
        return 0;

    if (waitpid (s->ssh_pid, &status, 0) < 0)
        err ("%p: %S: ssh pid %ld: %m\n", s->target, (long) s->ssh_pid);  

    if (status != 0)
        err ("%p: %s: ssh exited with exit code %d\n", 
             s->target, WEXITSTATUS (status));

    ssh_info_destroy (s);

    return WEXITSTATUS (status);
}

struct ssh_info_struct * ssh_info_create (pid_t pid, const char *host, int fd)
{
    struct ssh_info_struct *s = Malloc (sizeof (*s));
    s->ssh_pid = pid;
    s->target = Strdup (host);
    s->fd = fd;
    return (s);
}

void ssh_info_destroy (struct ssh_info_struct *s)
{
    if (s == NULL)
        return;

    Free ((void **)&s->target);
    Free ((void **)&s);
    return;
}


static int sshcmd_args_init (void)
{
    int i = 0;
    char *val = NULL;
    char *str = NULL;
    List args_list = NULL;

    if (!(val = getenv ("PDSH_SSH_ARGS"))) 
        val = "-2 -a -x";

    str = Strdup (val);
    args_list = list_split (" ", str);
    Free ((void **) &str);

    if ((val = getenv ("PDSH_SSH_ARGS_APPEND"))) {
        str = Strdup (val);
        List l = list_split (" ", str);
        Free ((void **) &str);

        while ((str = list_pop (l)))
            list_append (args_list, str);

        list_destroy (l);
    }

    /*
     * Allocate space in ssh_args for all args in args_list
     *  plus space for "-l user", ssh command name in arg[0],
     *  and command string and final NULL at end.
     */
    ssh_args_len = list_count (args_list) + 6;
    fprintf (stderr, "ssh_args_len = %d\n", ssh_args_len);
    ssh_args = Malloc (ssh_args_len *  sizeof (char *));

    memset (ssh_args, 0, ssh_args_len);

    ssh_args[0] = Strdup ("ssh");
    ssh_args[1] = Strdup ("-l");
    ssh_args[2] = NULL; /* placeholder for username */
    
    i = 3;
    while ((str = list_pop (args_list)))
        ssh_args[i++] = str;

    ssh_args_host_index = i;

    list_destroy (args_list);

    return (0);
}


static char **ssh_args_create (char *host, char *user, char *cmd)
{
    int i;
    char **args = Malloc (ssh_args_len * sizeof (char *));
    /*
     * Make thread-local copy of ssh_args, so we can modify hostname
     *  and possibly username and command.
     */

    for (i = 0; i < ssh_args_len; i++)
        args[i] = ssh_args[i];

    args[2]                     = user;           
    args[ssh_args_host_index]   = host;          
    args[ssh_args_host_index+1] = cmd;

    return (args);
}

static void ssh_args_destroy (char **args)
{
    Free ((void **) &args);
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

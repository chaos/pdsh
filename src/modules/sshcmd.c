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
#include "src/common/fd.h"
#include "src/pdsh/dsh.h"
#include "src/pdsh/mod.h"

#define HBUF_LEN	1024

#if STATIC_MODULES
#  define pdsh_module_info sshcmd_module_info
#  define pdsh_module_priority sshcmd_module_priority
#endif    

int pdsh_module_priority = DEFAULT_MODULE_PRIORITY;

struct ssh_info_struct {
    int ssh_pid;            /* PID of ssh command         */
    char *target;           /* Hostname of ssh target     */
    int fd;                 /* stderr fd to ssh command for signals  */
    int status;             /* Exit status of ssh command */
    int exited;
};

static pthread_mutex_t reaper_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t reaper_cond = PTHREAD_COND_INITIALIZER;
static pthread_t ssh_tid;
static int ssh_count;
static int first_ssh_started = 0;
static List ssh_list;


void ssh_info_append (pid_t pid, const char *host, int fd);
void ssh_info_destroy (struct ssh_info_struct *s);
struct ssh_info_struct * ssh_info_create (pid_t pid, const char *host, int fd);
struct ssh_info_struct * ssh_info_find_by_fd (int fd);
static void *ssh_reaper (void *arg);
static void _block_sigchld (void);
    
static int mod_ssh_postop(opt_t *opt);
static int mod_ssh_exit (void);

static int sshcmd_init(opt_t *);
static int sshcmd_signal(int, int);
static int sshcmd(char *, char *, char *, char *, char *, int, int *);

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
    (RcmdInitF)  sshcmd_init,
    (RcmdSigF)   sshcmd_signal,
    (RcmdF)      sshcmd,
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
        _block_sigchld ();
    }
    return 0;
}


static void
_drop_privileges()
{
    setuid(getuid());
    setgid(getgid());
}

static int sshcmd_init(opt_t * opt)
{
    pthread_attr_t attr;

    /*
     * Drop privileges if we're running setuid
     */
    _drop_privileges();

    ssh_count = hostlist_count (opt->wcoll);

    if ((ssh_list = list_create ((ListDelF) ssh_info_destroy)) == NULL)
        return -1;

    pthread_attr_init (&attr);
    pthread_create (&ssh_tid, &attr, &ssh_reaper, NULL);

    return 0;
}

static int mod_ssh_exit (void)
{
    void *rc;
    /* Clean up all kinds of funky junk */
    if (!ssh_list)
        return 0;

    pthread_join (ssh_tid, &rc);

    /*
     * As each member of the ssh_list is destroyed, an error message
     *  will be printed to the screen if exit code is nonzero.
     */
    list_destroy (ssh_list);

    fflush (stderr);
    return 0;
}

/*
 * SSH doesn't support signal forwarding, at least the way pdsh uses it
 *  at this time. Instead we always send SIGTERM which seems to have the
 *  desired effect of killing off ssh most of the time.
 */
static int sshcmd_signal(int fd, int signum)
{
    struct ssh_info_struct *s = ssh_info_find_by_fd (fd);
    if (s == NULL)
        return -1;

    /*
     *  Always send SIGTERM. SIGINT doesn't seem to get forwarded by ssh, and
     *    really termination of the connection is probably the desired result.
     */
    err ("sending SIGTERM to ssh %s pid %d\n", s->target, s->ssh_pid);
    return (kill (s->ssh_pid, SIGTERM));
}

/*
 * This is a replacement rcmd() function that uses an arbitrary
 * program in place of a direct rcmd() function call.
 */
static int _pipecmd(char *path, char *args[], const char *ahost, int *fd2p)
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

    /*
     * Hold the reaper lock so that the child doesn't exit before
     *  we have a chance to add its pid to the ssh_list
     */
    pthread_mutex_lock (&reaper_mutex);

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

        setsid();
        putenv("DISPLAY=");
        execvp(path, args);
        err("%p: execlp %s failed for %S: %m.", path, ahost);
        _exit(255);
    }
    if (cpid > 0) {
        /* parent. close sp[1], return sp[0]. */
        (void) close(sp[1]);

        /*
         * Set close on exec for sp[0] and esp[0]
         */
        fd_set_close_on_exec (sp[0]);

        if (fd2p) {
            close(esp[1]);
            *fd2p = esp[0];
            fd_set_close_on_exec (esp[0]);
        }
        /* reap child. */
        /* (void) wait(0); */

        ssh_info_append (cpid, ahost, esp[0]);

        /*
         * Now it is safe for the reaper to collect exit status
         *  of the child.
         */
        pthread_mutex_unlock (&reaper_mutex);

        return sp[0];
    }
    /*NOTREACHED*/ return 0;
}

static int
sshcmdrw(char *ahost, char *addr, char *luser, char *ruser, char *cmd,
       int rank, int *fd2p)
{
    char *prog = "ssh"; /* xbasename(_PATH_SSH); */
    char *args[] = { 0, "-q", "-a", "-x", "-l", 0, 0, 0, 0, 0, 0 };

    args[0] = prog;
    args[5] = ruser;            /* solaris cc doesn't like non constant */
    args[6] = ahost;            /*     initializers */
    args[7] = cmd;

    return _pipecmd("ssh", args, ahost, fd2p);
}

static int
sshcmd(char *ahost, char *addr, char *luser, char *ruser, char *cmd,
         int rank, int *fd2p)
{
    int rc;

    rc = sshcmdrw(ahost, addr, luser, ruser, cmd, rank, fd2p);

    pthread_mutex_lock (&reaper_mutex);
    if (first_ssh_started == 0) {
        first_ssh_started = 1;
        pthread_cond_signal (&reaper_cond);
    }
    pthread_mutex_unlock (&reaper_mutex);

    return rc;
}

struct ssh_info_struct * ssh_info_create (pid_t pid, const char *host, int fd)
{
    struct ssh_info_struct *s = Malloc (sizeof (*s));
    s->ssh_pid = pid;
    s->target = Strdup (host);
    s->fd = fd;
    s->exited = 0;
    s->status = -1;
    return (s);
}

void ssh_info_destroy (struct ssh_info_struct *s)
{
    if (s == NULL)
        return;

    if (s->status != 0)
        err ("%p: %s: ssh exited with exit code %d\n", 
             s->target, WEXITSTATUS (s->status));

    Free ((void **)&s->target);
    Free ((void **)&s);
    return;
}

void ssh_info_append (pid_t pid, const char *host, int fd)
{
    struct ssh_info_struct *s = ssh_info_create (pid, host, fd);
    list_append (ssh_list, s);
    return;
}

static int cmp_fd (struct ssh_info_struct *s, int *fdp)
{
    return (s->fd == *fdp);
}

struct ssh_info_struct * ssh_info_find_by_fd (int fd)
{
    return (list_find_first (ssh_list, (ListFindF) cmp_fd, &fd));
}

static int cmp_pid (struct ssh_info_struct *s, pid_t *pidp)
{
    return (s->ssh_pid == *pidp);
}

static void *ssh_reaper (void *arg)
{
    sigset_t set;

    /*
     * Wait for signal that we're starting
     */
    pthread_mutex_lock (&reaper_mutex);
    if (!first_ssh_started)
        pthread_cond_wait (&reaper_cond, &reaper_mutex);
    pthread_mutex_unlock (&reaper_mutex);

    if (ssh_count <= 0)
        return NULL;

    sigemptyset (&set);
    sigaddset (&set, SIGCHLD);

    while (ssh_count) {
        int status = 0;
        pid_t pid = -1; 
        struct ssh_info_struct *s;

        if ((pid = waitpid (-1, &status, 0)) < 0) {
            if (errno == ECHILD) {
                err ("%p: ssh: ssh_count = %d and ECHILD\n", ssh_count);
                break;
            }
            err ("%p: ssh: waitpid: %m\n");
            continue;
        }

        pthread_mutex_lock (&reaper_mutex);
        if (!(s = list_find_first (ssh_list, (ListFindF) cmp_pid, &pid))) {
            err ("%p: ssh: pid %d exited but wasn't an ssh cmd\n", (int) pid);
            pthread_mutex_unlock (&reaper_mutex);
            continue;
        }
        pthread_mutex_unlock (&reaper_mutex);

        s->exited = 1;
        s->status = status;
        ssh_count--;
    }

    return NULL;
}

static void _block_sigchld (void)
{
    sigset_t set;
    sigemptyset (&set);
    sigaddset (&set, SIGCHLD);
    pthread_sigmask (SIG_BLOCK, &set, NULL);
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

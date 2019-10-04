/*****************************************************************************\
 *  $Id$
 *****************************************************************************
 *  Copyright (C) 2007 The Regents of the University of California.
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

#if     HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/wait.h>
#include <sys/socket.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>

#include "src/common/xmalloc.h"
#include "src/common/xstring.h"
#include "src/common/err.h"
#include "src/common/list.h"
#include "src/common/split.h"
#include "src/pdsh/dsh.h"
#include "src/pdsh/mod.h"

#include "src/common/pipecmd.h"

struct pipe_info_struct {
    pid_t pid;              /* pid of child command             */
    char *path;             /* path to executable               */
    char *cmd;              /* basename of path                 */
    char *target;           /* Target host of this instance     */
    char *username;         /* Username for this instance       */
    char **args;            /* Cmd arguments                    */
    int rank;               /* Rank 1-N of this instance        */
    int fd;                 /* stdin/out fd                     */
    int efd;                /* stderr fd                        */
};

static int _pipecmd (char *path, char *args[], int *fd2p, pid_t *ppid);

pipecmd_t pipe_info_create (const char *path, const char *target, 
        const char *user, int rank)
{
    struct pipe_info_struct *e = Malloc (sizeof (*e));

    e->path = Strdup (path);
    e->cmd = Strdup (xbasename (e->path));
    e->target = Strdup (target);
    e->username = Strdup (user);
    e->rank = rank;

    e->pid = (pid_t) 0;
    e->args = NULL;
    e->fd = -1;
    e->efd = -1;

    return (e);
}

static void pipe_info_destroy (struct pipe_info_struct *e)
{
    if (e == NULL)
        return;

    Free ((void **) &e->path);
    Free ((void **) &e->cmd);
    Free ((void **) &e->target);
    Free ((void **) &e->username);
    Free ((void **) &e);
}

static char * pipecmd_format_arg (pipecmd_t e, const char *arg)
{
    char buf [64];
    const char *p;
    char *str = NULL;

    p = arg;
    while (*p != '\0') {
        if (*p == '%') {
            p++;
            switch (*p) {
                case 'h' : /* '%n' => target name */
                    xstrcat (&str, e->target);
                    break;
                case 'u':
                    xstrcat (&str, e->username);
                    break;
                case 'n':
                    snprintf (buf, sizeof (buf) - 1, "%d", e->rank);
                    xstrcat (&str, buf);
                    break;
                case '%':
                    xstrcatchar (&str, '%');
                    break;
                default:
                    xstrcatchar (&str, '%');
                    xstrcatchar (&str, *p);
                    break;
            }
        }
        else 
            xstrcatchar (&str, *p);
        p++;
    }

    return (str);
}

/*
 *  Loop through argv expanding any format characters
 */
static char ** cmd_args_create (pipecmd_t e, const char **argv)
{
    int i = 0;
    int n = 0;
    char **args = NULL;

    while (argv[n])
        n++;

    args = Malloc ((n + 2) * sizeof (char *));
    /*
     *  Cmd should be args[0]:
     */
    args [0] = Strdup (e->cmd);
    for (i = 1; i < n+1; i++) 
        args[i] = pipecmd_format_arg (e, argv[i-1]);

    args[i] = NULL;

    return (args);
}

static void cmd_args_destroy (char **args)
{
    int i = 0;

    if (args == NULL)
        return;

    while (args[i])
        Free ((void **) &args[i++]);

    Free ((void **) &args);
}

void pipecmd_destroy (pipecmd_t p)
{
    cmd_args_destroy (p->args);
    pipe_info_destroy (p);
    return;
}

pipecmd_t pipecmd (const char *path, const char **args, const char *target,
        const char *user, int rank)
{
    pipecmd_t p = pipe_info_create (path, target, user, rank);
    p->args = cmd_args_create (p, args);

    if ((p->fd = _pipecmd (p->path, p->args, &p->efd, &p->pid)) < 0) {
        err ("%p: exec cmd %s failed for host %S\n", path, target);
        pipecmd_destroy (p);
        return (NULL);
    }
    return (p);
}

int pipecmd_stdoutfd (pipecmd_t p)
{
    if (p == NULL)
        return (-1);
    return (p->fd);
}

int pipecmd_stderrfd (pipecmd_t p)
{
    if (p == NULL)
        return (-1);
    return (p->efd);
}

int pipecmd_signal (pipecmd_t p, int signo)
{
    char *cmd;

    if (p == NULL)
        return (-1);
   
    cmd =  xbasename (p->path);
    err ("sending signal %d to %s [%s] pid %d\n", signo, p->target, cmd,
            p->pid);

    return (kill (p->pid, signo));
}

int pipecmd_wait (pipecmd_t p, int *pstatus)
{
    int status = 0;

    if (p == NULL)
        return (-1);

    if (waitpid (p->pid, &status, 0) < 0)
        err ("%p: %S: %s pid %ld: waitpid: %m\n", p->target, 
                xbasename (p->path), p->pid);

    if (status != 0) {
        if (WIFEXITED (status))
            err ("%p: %S: %s exited with exit code %d\n",
                 p->target, xbasename (p->path), WEXITSTATUS (status));
        else if (WIFSIGNALED (status))
            err ("%p: %S: %s killed by signal %d\n",
                 p->target, xbasename (p->path), WTERMSIG (status));
        else
            err ("%p: %S: %s exited with nonzero status 0x%04x\n",
                 p->target, xbasename (p->path), status);
    }

    if (pstatus)
        *pstatus = status;

    return (0);
}

const char * pipecmd_target (pipecmd_t p)
{
    return (p->target);
}


static void closeall (int fd)
{
    int fdlimit = sysconf (_SC_OPEN_MAX);

    while (fd < fdlimit)
        close (fd++);
    return;
}

static int _pipecmd (char *path, char *args[], int *fd2p, pid_t *ppid)
{
    int sp[2], esp[2];

    /*
     *  Get socketpair for stdin/out
     */
    if (socketpair (AF_UNIX, SOCK_STREAM, 0, sp) < 0) {
        err ("%p: pipecmd: socketpair: %m\n");
        return (-1);
    }

    if (fd2p && socketpair (AF_UNIX, SOCK_STREAM, 0, esp) < 0) {
        err ("%p: pipecmd: socketpair: %m\n");
        return (-1);
    }

    if ((*ppid = fork ()) < 0) {
        err ("%p: pipecmd: fork: %m\n");
        return (-1);
    }

    if (*ppid == 0) {
        /*
         *  Child. We use sp[1] for stdin/out, and close sp[0]
         */
        (void) close (sp[0]);
        if ((dup2 (sp[1], 0) < 0) || (dup2 (0, 1) < 0)) {
            err ("%p: pipecmd (in child): dup2: %m");
            _exit (255);
        }

        /*
         *  Dup seperate stderr socketpair if fd2p was passed in.
         *   Otherwise dup stdin/out onto stderr.
         */
        if (dup2 ((fd2p ? esp[1] : 0), 2) < 0) {
                err ("%p: pipecmd (in child): dup2: %m");
                _exit (255);
        }
        if (fd2p)
            (void) close (esp[0]);

        /*  Try to close all stray file descriptors before
         *   invocation of cmd to ensure that cmd stdin is closed
         *   when pdsh/pdcp close their end of the socketpair.
         */
        closeall (3);

        setsid ();
        execvp (path, args);
        err ("%p: execvp %s failed: %m\n", path);
        _exit (255);
    }

    /*
     * Parent continues
     */
    (void) close (sp[1]);
    if (fd2p) {
        (void) close (esp[1]);
        *fd2p = esp[0];
    }

    return (sp[0]);
}


/*
 * vi: ts=4 sw=4 expandtab
 */

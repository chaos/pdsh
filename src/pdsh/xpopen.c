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

#if     HAVE_CONFIG_H
#include "config.h"
#endif

#if	HAVE_UNISTD_H
#include <unistd.h>             /* for R_OK, access() */
#endif

#include <stdio.h>
#include <limits.h>             /* ARG_MAX */
#include <sys/wait.h>           /* waitpid() */
#include <string.h>             /* strcmp() */
#include <stdlib.h>
#include <errno.h>

#include "xmalloc.h"
#include "xstring.h"
#include "err.h"
#include "list.h"

#define QUOTE '\"'
#define SPACE ' '
#define TAB   '\t'
#define NWLN  '\n'

extern int errno;

static struct pid {
    struct pid *next;
    FILE *fp;
    pid_t pid;
} *pidlist;

/* forward declaration */
static void _parse_command_with_quotes(char *av[], int maxargs, char *str);

/* 
 * xpopen(): a safer popen for pdsh.
 * We bypass shell by doing a fork and exec'ing the cmd directly.
 * also, set euid back to original user. This avoids passing possibly
 * user supplied arguments to a suid shell.
 *
 * xpopen returns NULL if the fork or pipe calls fail, or if it cannot
 * allocate memory.
 *
 * Since a shell is not invoked on the command string, you cannot pass
 * things that normally the shell would handle. The only thing xpopen()
 * attempts to deal with are double quoted strings.
 *
 * the returned stream must be closed by xpclose (see below)
 * 
 * cmd (IN) 	cmd to run, as in popen
 * mode(IN)	"r" for reading, "w" (or anything else) for writing
 * OUT		FILE * to output/input stream of child process
 */
FILE *xpopen(char *cmd, char *mode)
{
    struct pid *cur;
    int fds[2], read, fd;
    pid_t pid;
    char *av[ARG_MAX + 1];
    int maxfd = sysconf(_SC_OPEN_MAX);
    
    _parse_command_with_quotes(av, ARG_MAX, cmd);

    if ((*mode != 'r' && *mode != 'w') || mode[1] != '\0') {
        errno = EINVAL;
        return (NULL);
    }

    cur = Malloc(sizeof(struct pid));

    read = (*mode == 'r');

    if (pipe(fds) < 0) {
        close(fds[0]);
        close(fds[1]);
        Free((void **) &cur);
        errx("%p: unable to dup stdout\n");
    }

    switch (pid = fork()) {
    case -1:                   /* Error. */
        close(fds[0]);
        close(fds[1]);
        Free((void **) &cur);
        return (NULL);

    case 0:                    /* child */

        close(fds[read ? 0 : 1]);
        dup2(fds[read ? 1 : 0], read ? STDOUT_FILENO : STDIN_FILENO);

        for (fd = STDERR_FILENO + 1; fd < maxfd; fd++)
            close(fd);

        setgid(getgid());
        setuid(getuid());

        do {
            if (access(av[0], F_OK) != 0 && errno != EINTR) {
                fprintf(stderr, "%s: not found\n", av[0]);
                fflush(stderr);
            }
        } while (errno == EINTR);

        execv(av[0], av);

        exit(errno);
    }                           /* switch() */

    close(fds[read ? 1 : 0]);

    /* insert child pid into pidlist */
    cur->fp = fdopen(fds[read ? 0 : 1], mode);
    cur->pid = pid;
    cur->next = pidlist;
    pidlist = cur;

    return (cur->fp);

}

/*
 * xpclose(): close stream opened with xpopen. reap proper child and
 *            return exit status.
 *
 * f (IN)	file stream as returned by xpopen()
 *
 * (OUT)	-1 on error (was f opened by xpopen?) 
 *              otherwise, exit status of child as modified by 
 *              WEXITSTATUS macro (see waitpid(3))
 *              This is different from pclose, which returns the 
 *              unmodified status from waitpid.
 */
int xpclose(FILE * f)
{
    int status;
    pid_t pid;
    struct pid *cur, *last;

    fclose(f);

    for (last = NULL, cur = pidlist; cur; last = cur, cur = cur->next)
        if (f == cur->fp)
            break;

    if (cur == NULL)
        return (-1);

    do {
        pid = waitpid(cur->pid, &status, 0);
    } while (pid == -1 && errno == EINTR);

    if (last == NULL)
        pidlist = cur->next;
    else
        last->next = cur->next;

    Free((void **) &cur);

    return (WIFEXITED(status) != 0) ? WEXITSTATUS(status) : -1;
}

/* parse_commaned_with_quotes(): 
 * helper function for xpopen
 *
 * provides simple string parsing with support for double quoted arguments,
 * and that is all. This is probably too simple to do what you want.
 * Only makes a minimal effort to complain when there's no matching quote.
 *
 * argv(IN) container for resulting arguments, strings enclosed in "" are
 *          treated as a single arg.
 * maxn(IN) max number of arguments
 * str(IN)	string to parse
 *
 */
static void _parse_command_with_quotes(char *argv[], int maxn, char *str)
{
    int i = 0;
    char *c, *lc;

    c = lc = str;

    while (*c != '\0' && i < maxn) {
        switch (*c) {
        case QUOTE:
            lc = ++c;
            /* find matching quote */
            while (*c != '\0' && *c != QUOTE)
                c++;

            if (*c == '\0')
                errx("%P: Unmatched `%c' in xpopen\n", *lc);

            /* nullify quote */
            *c = '\0';

            /* push token onto argument vector */
            if (strlen(lc) > 0)
                argv[i++] = lc;

            /* move c past null */
            lc = ++c;

            break;
        case SPACE:
        case TAB:
        case NWLN:
            /* nullify and push token onto list */
            *c = '\0';
            if (lc != NULL && strlen(lc) > 0)
                argv[i++] = lc;

            lc = ++c;
            break;
        default:
            c++;
        }
    }

    /* hit a null. push last token onto list, if such a one exists */
    if (strlen(lc) > 0 && i < maxn)
        argv[i++] = lc;

    argv[i] = NULL;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

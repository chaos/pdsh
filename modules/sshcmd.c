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

#include "xstring.h"
#include "err.h"
#include "dsh.h"
#include "list.h"
#include "mod.h"

#define HBUF_LEN	1024

#if STATIC_MODULES
#  define pdsh_module_info sshcmd_module_info
#endif    

/*
 * use_rw will be set to true for PCP mode
 */
static bool use_rw = false;

static int mod_ssh_postop(opt_t *opt);

static int sshcmd_init(opt_t *);
static int sshcmd_signal(int, int);
static int sshcmd(char *, char *, char *, char *, char *, int, int *);

/*
 *  Export generic pdsh module operations:
 */
struct pdsh_module_operations sshcmd_module_ops = {
    (ModInitF)       NULL,
    (ModExitF)       NULL,
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
    }

    return 0;
}


static void
_drop_privileges()
{
    if (geteuid() == (uid_t) 0) {
        setuid(getuid());
        setgid(getgid());
    }
}

static int sshcmd_init(opt_t * opt)
{
    if (opt->personality == PCP)
        use_rw = true;

    /*
     * Drop privileges if we're running setuid
     */
    _drop_privileges();

    return 0;
}

static int sshcmd_signal(int fd, int signum)
{
    /* not implemented */
    return 0;
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
        /* heroic measures to prevent ssh from prompting for pphrase */
        setsid();
        putenv("DISPLAY=");

        /* fork again to lose parent. */
        cpid = fork();
        if (cpid < 0) {
            err("%p: fork to lose parent failed for %S: %m\n", ahost);
            _exit(255);
        }
        if (cpid > 0)
            _exit(0);
        /* in grandchild here. */
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
        /* reap child. */
        (void) wait(0);
        return sp[0];
    }
    /*NOTREACHED*/ return 0;
}

/* pdsh uses this version */
static int
sshcmdr(char *ahost, char *addr, char *luser, char *ruser, char *cmd,
       int rank, int *fd2p)
{
    char *prog = "ssh"; /* xbasename(_PATH_SSH); */
    char *args[] = { 0, "-q", "-a", "-x", "-f", "-l", 0, 0, 0, 0 };

    /* -f args changed for ssh2 (-fo didn't do the same thing as ssh1 -f) */
    if (strcmp(prog, "ssh2") == 0)
        args[4] = "-oBatchMode yes";

    args[0] = prog;
    args[6] = ruser;            /* solaris cc doesn't like non constant */
    args[7] = ahost;            /*     initializers */
    args[8] = cmd;

    return _pipecmd("ssh", args, ahost, fd2p);
}

/* pdcp uses this version */
static int
sshcmdrw(char *ahost, char *addr, char *luser, char *ruser, char *cmd,
         int rank, int *fd2p)
{
    char *prog = "ssh"; /* xbasename(_PATH_SSH); */
    char *args[] = { 0, "-q", "-a", "-x", "-l", 0, 0, 0, 0 };

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
	if (use_rw)
		return sshcmdrw(ahost, addr, luser, ruser, cmd, rank, fd2p);
	else
		return sshcmdr(ahost, addr, luser, ruser, cmd, rank, fd2p);
}

#if 0
static int rshcmd(char *ahost, char *luser, char *ruser, char *cmd, int *fd2p)
{
    char *args[] = { "rsh", 0, "-l", 0, 0, 0 };

    args[1] = ahost;            /* solaris cc doesn't like non constant */
    args[3] = ruser;            /*     initializers */
    args[4] = cmd;

    return _pipecmd(_PATH_RSH, args, ahost, fd2p);
}
#endif

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

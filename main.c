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

#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#if	HAVE_UNISTD_H
#include <unistd.h>             /* seteuid */
#endif
#include <sys/wait.h>           /* wait */
#include <string.h>             /* strcmp */
#include <stdlib.h>             /* exit */
#include <stdio.h>

#if	HAVE_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

#include "err.h"
#include "xmalloc.h"
#include "xstring.h"
#include "dsh.h"
#include "opt.h"
#include "mod.h"

extern const char *pdsh_module_dir;

static void _interactive_dsh(opt_t *);

int main(int argc, char *argv[])
{
    opt_t opt;
    int retval = 0;

    /*
     * Initialize.
     */
    err_init(xbasename(argv[0]));       /* init err package */

    mod_init();

    /*
     *  XXX: Need a better way to do this.
     */
    if (mod_load_modules(pdsh_module_dir) < 0)
        errx("%p: Couldn't load any pdsh modules\n");


    /*
     * Handle options.
     */
    opt_default(&opt);          /* set options to default values */
    opt_env(&opt);              /* override with env variables */
    opt_args(&opt, argc, argv); /* override with command line */

    if (opt_verify(&opt)) {     /* verify options, print errors */

        /*
         * Do the work.
         */
        if (opt.info_only)      /* display info only */
            opt_list(&opt);
        else if (opt.personality == PCP || opt.cmd != NULL)
            retval = dsh(&opt); /* single dsh/pcp command */
        else                    /* prompt loop */
            _interactive_dsh(&opt);
    } else {
        retval = 1;
    }

    mod_exit(); 

    /*
     * Clean up.
     */
    opt_free(&opt);             /* free heap storage in opt struct */
    err_cleanup();

    return retval;
}

#if	HAVE_READLINE

/* Warning: Running setuid root! */
static void _interactive_dsh(opt_t * opt)
{
    pid_t pid;
    char prompt[64];
    char *cmd = NULL;

    snprintf(prompt, sizeof(prompt), "%s> ", opt->progname);

/*rl_bind_key('\t', rl_insert); *//* disable tab filename expansion */

    while ((cmd = readline(prompt)) != NULL) {
        if (strlen(cmd) == 0) { /* empty line */
            free(cmd);
            continue;
        }
        if (!strcmp(cmd, "quit") || !strcmp(cmd, "exit")) {
            free(cmd);          /* quit or exit */
            break;
        }
        add_history(cmd);

        /* 
         * fork dsh so we can ignore SIGINT in prompt loop 
         */
        switch (pid = fork()) {
        case -1:               /* error */
            errx("%p: fork: %m\n");
        case 0:                /* child - run cmd */
            opt->cmd = Strdup(cmd);
            dsh(opt);
            Free((void **) &opt->cmd);
            exit(0);
        default:               /* parent - wait */
            while (waitpid(pid, NULL, 0) < 0) {
                if (errno != EINTR)
                    break;
            }
            break;
        }

        free(cmd);
    }
}

#else

static char *_getcmd(char *);
static void _shell(uid_t, char *);

/*
 * Prompt for dsh commands, then execute them, prompt again, ...
 * "quit", "exit", or ^D to get out.
 *	opt (IN)	program options struct
 */
static void _interactive_dsh(opt_t * opt)
{
    pid_t pid;

    signal(SIGINT, SIG_IGN);

    while ((opt->cmd = _getcmd(opt->progname))) {
        if (*opt->cmd == '\0') {        /* empty command */
            Free((void **) &opt->cmd);
            continue;
        }
        if (*opt->cmd == '!') { /* shell escape */
            _shell(opt->luid, opt->cmd + 1);
            Free((void **) &opt->cmd);
            continue;
        }
        if (strcmp(opt->cmd, "quit") == 0       /* user exit */
            || strcmp(opt->cmd, "exit") == 0) {
            Free((void **) &opt->cmd);
            break;
        }

        /* must fork dsh so we can ignore SIGINT in prompt loop */
        switch (pid = fork()) {
        case -1:
            errx("%p: fork: %m\n");
        case 0:
            dsh(opt);           /* run command */
            exit(0);
        default:
            while (waitpid(pid, NULL, 0) < 0 && errno == EINTR);
        }
        Free((void **) &opt->cmd);
    }
}

/*
 * Run a command that was shell-escaped from the dsh> prompt.  Run it as
 * the real uid of the invoking user, so we must fork to maintain root 
 * effective uid in the parent. 
 *	uid (IN)	uid used to execute command
 *	cmd (IN)	command and args
 */
static void _shell(uid_t uid, char *cmd)
{
    pid_t pid;

    switch (pid = fork()) {
    case -1:
        errx("%p: fork: %m\n");
    case 0:
        seteuid(uid);
        system(cmd);
        exit(0);
    default:
        waitpid(pid, NULL, 0);
    }
}

/* 
 * Prompt for a command and return it.  
 *	prompt (IN)	string used to build prompt (e.g. program name)
 */
static char *_getcmd(char *prompt)
{
    char *cmd = NULL;
    char buf[LINEBUFSIZE];

    out("%s> ", prompt);
    if (fgets(buf, LINEBUFSIZE, stdin) != NULL) {
        buf[LINEBUFSIZE - 1] = '\0';
        xstrcln(buf, NULL);
        cmd = Strdup(buf);
    }
    return cmd;
}

#endif                          /* WITH_READLINE */

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

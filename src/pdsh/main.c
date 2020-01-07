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

#if     HAVE_CONFIG_H
#include "config.h"
#endif

#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/param.h>
#if	HAVE_UNISTD_H
#include <unistd.h>             /* setuid */
#endif
#include <sys/wait.h>           /* wait */
#include <string.h>             /* strcmp */
#include <stdlib.h>             /* exit */
#include <stdio.h>

#if	HAVE_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif

#include "src/common/err.h"
#include "src/common/xmalloc.h"
#include "src/common/xstring.h"
#include "dsh.h"
#include "opt.h"
#include "mod.h"
#include "pcp_client.h"
#include "pcp_server.h"
#include "privsep.h"

extern const char *pdsh_module_dir;

static void _interactive_dsh(opt_t *);
static int _pcp_remote_client (opt_t *);
static int _pcp_remote_server (opt_t *);

int main(int argc, char *argv[])
{
    opt_t opt;
    int retval = 0;
    const char *m;
    char *prog = xbasename(argv[0]);

#if HAVE_READLINE
    rl_readline_name = prog;
#endif

    /*
     * Initialize.
     */
    err_init(prog);       /* init err package */

    /*
     *  If running setuid, fork a child to handle 
     *   all privileged operations and drop privs in this process.
     */
    privsep_init();

    /*
     * Seed options with default values:
     */
    opt_default(&opt, argv[0]);

    /*
     * Override defaults with environment
     */
    opt_env(&opt);

    /*
     * Process any options that need to be handled early:
     */
    opt_args_early(&opt, argc, argv);

    /*
     *  Load static or dynamic pdsh modules
     */
    mod_init();
    /*
     *  Allow module directory to be overridden, but not when
     *   running as root or setuid. (This is mainly for testing...)
     */
    if (!(m = getenv ("PDSH_MODULE_DIR")) ||
          getuid() == 0 ||
          getuid() != geteuid())
        m = pdsh_module_dir;
    if (mod_load_modules(m, &opt) < 0)
        errx("%p: Couldn't load any pdsh modules\n");

    /*
     * Handle options.
     */
    opt_args(&opt, argc, argv); /* override with command line           */

    if (opt_verify(&opt)) {     /* verify options, print errors         */
        /*
         * Do the work.
         */
        if (opt.info_only)      /* display info only */
            opt_list(&opt);
        else if (pdsh_personality() == PCP && opt.pcp_server) 
            retval = (_pcp_remote_server (&opt) < 0);
        else if (pdsh_personality() == PCP && opt.pcp_client)
            retval = (_pcp_remote_client (&opt) < 0);
        else if (pdsh_personality() == PCP || opt.cmd != NULL)
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
    privsep_fini();
    opt_free(&opt);             /* free heap storage in opt struct */
    err_cleanup();

    return retval;
}

#if	HAVE_READLINE

static int _history_file_create (char *path, size_t len)
{
    char *      home;
    struct stat sbuf;
    int         fd;
    int         rc;
    int         n;

    memset (path, 0, len);

    if (!(home = getenv ("HOME"))) {
        err ("%p: Unable to read HOME env var\n");
        return (-1);
    }

    n = snprintf (path, len, "%s/.pdsh", home);
    if ((n < 0) || (n >= len)) {
        err ("%p: Unable to open %s/.pdsh: path too long\n", home);
        return (-1);
    }

    /*  Check for ~/.pdsh directory 
     *    and create if it does not exist
     */
    if (lstat (path, &sbuf) < 0) {
        if (errno == ENOENT) {
            if (mkdir (path, 0700) < 0) {
                err ("%p: Unable to create ~/.pdsh: %m\n");
                return (-1);
            }
        } else {
            err ("%p: Couldn't find ~/.pdsh: %m\n");
            return (-1);
        }
    } else if (!(S_ISDIR (sbuf.st_mode))) {
        err ("%p: ~/.pdsh exists but is not a directory\n");
        return (-1);
    }

    /*  Now should have ~/.pdsh/,
     *    create ~/.pdsh/history if it does not exist.
     */
    n = snprintf (path, len, "%s/.pdsh/history", home);
    if ((n < 0) || (n >= len)) {
        err ("%p: Unable to open %s/.pdsh/history: path too long\n", home);
        return (-1);
    }

    if ((fd = open (path, O_CREAT, 00600)) < 0) {
        err ("%p: Error: Unable to create history file \"%s\": %m\n", path);
        return (-1);
    }

    close (fd);

    if ((rc = read_history (path))) {
        err ("%p: Warning: Unable to read history file \"%s\": %s\n", 
                path, strerror (rc));
        return (-1);
    }

    return (0);
}

static void _history_list (void)
{
    HIST_ENTRY **list;
    int          i;

    if (!(list = history_list ()))
        return;

    for (i = 0; list[i]; i++) {
        out ("%p: %d: %s\n", i + history_base, list[i]->line);
    }
    return;
}

/* Warning: May be running setuid root! */
static void _interactive_dsh(opt_t * opt)
{
    pid_t pid;
    char prompt[64];
    char history_filename[MAXPATHLEN];
    char *cmd = NULL;
    int got_history_file = 1;
    int len;

    snprintf(prompt, sizeof(prompt), "%s> ", opt->progname);

    using_history ();

    len = sizeof (history_filename);

    if (_history_file_create (history_filename, len) < 0) {
        got_history_file = 0;
    } 

    while ((cmd = readline(prompt)) != NULL) {
        int   errnum;
        char *expansion;

        if ((errnum = history_expand (cmd, &expansion))) {
            err ("%p: %s\n", expansion);
        }

        free (cmd);

        if ((errnum < 0) || (errnum == 2)) {
            free (expansion);
            continue;
        }

        cmd = expansion;
 
        if (!strcmp(cmd, "history")) {
            _history_list ();
            continue;
        }

        add_history (cmd);

        if (strlen(cmd) == 0) { /* empty line */
            free(cmd);
            continue;
        }
        if (!strcmp(cmd, "quit") || !strcmp(cmd, "exit")) {
            free(cmd);          /* quit or exit */
            break;
        }

        if ((strlen(cmd) != 0) && (got_history_file)) 
            append_history (1, history_filename);

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

        free (cmd);
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
    int ret;
    pid_t pid;

    switch (pid = fork()) {
    case -1:
        errx("%p: fork: %m\n");
    case 0:
        if (setuid(uid) < 0)
            errx ("%p: setuid: %m\n");
        ret = system(cmd);
        if (ret)
            err ("%p: exited with status %d\n", ret);
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

static int _pcp_remote_server (opt_t *opt)
{
    struct pcp_server svr[1];

    svr->infd =          STDIN_FILENO;
    svr->outfd =         STDOUT_FILENO;
    svr->preserve =      opt->preserve;
    svr->target_is_dir = opt->target_is_directory;
    svr->outfile =       opt->outfile_name;

    return (pcp_server (svr));
}

static int _pcp_remote_client (opt_t *opt)
{
    struct pcp_client pcp[1];

    pcp->infd =  STDIN_FILENO;
    pcp->outfd = STDOUT_FILENO;

    pcp->infiles = pcp_expand_dirs (opt->infile_names);

    pcp->host =       opt->pcp_client_host;
    pcp->preserve =   opt->preserve;
    pcp->pcp_client = opt->pcp_client;

    return (pcp_client (pcp));
}


/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

/* 
 * $Id$ 
 * 
 * Copyright (C) 2000-2002 Regents of the University of California
 * See ./DISCLAIMER
 */

#if     HAVE_CONFIG_H
#include "config.h"
#endif

#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#if	HAVE_UNISTD_H
#include <unistd.h>	/* seteuid */
#endif
#include <sys/wait.h>	/* wait */
#include <string.h>	/* strcmp */
#include <stdlib.h>	/* exit */

#include "err.h"
#include "xmalloc.h"
#include "xstring.h"
#include "dsh.h"
#include "opt.h"

static char *getcmd(char *);
static void shell(uid_t, char *);
static void interactive_dsh(opt_t *);

int 
main(int argc, char *argv[])
{
	opt_t opt;
	int retval = 0;

	/*
	 * Initialize.
	 */
	err_init(xbasename(argv[0]));	/* init err package */

	/*
	 * Handle options.
	 */
	opt_default(&opt);		/* set options to default values */
	opt_env(&opt);			/* override with env variables */
	opt_args(&opt, argc, argv);	/* override with command line */

	if (opt_verify(&opt)) {		/* verify options, print errors */

		/*
		 * Do the work.
		 */
		if (opt.info_only)	/* display info only */
			opt_list(&opt);
		else if (opt.personality == PCP || opt.cmd != NULL) 
			retval = dsh(&opt); /* single dsh/pcp command */
		else  			/* prompt loop */ 
			interactive_dsh(&opt);
	} else {
		retval = 1;
	}

	/*
	 * Clean up.
	 */
	opt_free(&opt);			/* free heap storage in opt struct */

	return retval;
}

/*
 * Prompt for dsh commands, then execute them, prompt again, ...
 * "quit", "exit", or ^D to get out.
 *	opt (IN)	program options struct
 */
static void 
interactive_dsh(opt_t *opt)
{
	pid_t pid;

	signal(SIGINT, SIG_IGN);

	while ((opt->cmd = getcmd(opt->progname)))  {
		if (*opt->cmd == '\0') { 		/* empty command */
			Free((void **)&opt->cmd);
			continue;
		}
		if (*opt->cmd == '!') { 		/* shell escape */
			shell(opt->luid, opt->cmd + 1);
			Free((void **)&opt->cmd);
			continue;
		}
		if (strcmp(opt->cmd, "quit") == 0 	/* user exit */
		    || strcmp(opt->cmd, "exit") == 0) {
			Free((void **)&opt->cmd);
			break;
		}

		/* must fork dsh so we can ignore SIGINT in prompt loop */
		switch (pid = fork()) {
		case -1: 
			errx("%p: fork: %m\n");
		case 0:
			dsh(opt);			/* run command */
			exit(0);
		default:
			while (waitpid(pid, NULL, 0) < 0 && errno == EINTR)
				;
		}
		Free((void **)&opt->cmd);
	}
}

/*
 * Run a command that was shell-escaped from the dsh> prompt.  Run it as
 * the real uid of the invoking user, so we must fork to maintain root 
 * effective uid in the parent. 
 *	uid (IN)	uid used to execute command
 *	cmd (IN)	command and args
 */
static void 
shell(uid_t uid, char *cmd)
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
static char *
getcmd(char *prompt)
{
	char *cmd = NULL, *buf_cln;
	char buf[LINEBUFSIZE];

	out("%s> ", prompt);
	if (fgets(buf, LINEBUFSIZE, stdin) != NULL) {
		buf[LINEBUFSIZE - 1] = '\0';
		buf_cln = buf;
		xstrcln(&buf_cln, NULL);

		cmd = Strdup(buf_cln);
	}
	return cmd;
}


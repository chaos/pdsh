/* 
 * $Id$ 
 * 
 * Copyright (C) 2000 Regents of the University of California
 * See ./DISCLAIMER
 */

#include <conf.h>

#include <string.h>	/* for strcpy() */
#include <stdlib.h>	/* for getenv() */
#include <pwd.h>	/* for getpwuid() */

#include <sys/types.h>
#include <sys/stat.h>	/* for stat() */

#include "dsh.h"
#include "opt.h"
#include "err.h"
#include "list.h"
#include "wcoll.h"
#include "xstring.h"

static void usage(opt_t *opt);
static void show_version(void);

#define OPT_USAGE_DSH "\
Usage: pdsh [-options] command ...\n\
-s                separate stderr and stdout\n\
-S                return largest of remote command return values\n"

#define OPT_USAGE_PCP "\
Usage: dcp [-options] src [src2...] dest\n\
-r                recursively copy files\n\
-p                preserve modification time and modes\n"

#define OPT_USAGE_COMMON "\
-a                target all nodes\n\
-i                request alternate hostnames\n\
-q                list the option settings and quit\n\
-b                disable ^C status feature\n\
-l user           execute remote commands as user\n\
-t seconds        set connect timeout (default is 5 sec)\n\
-u seconds        set command timeout (no default)\n\
-f n              use fanout of n nodes\n\
-w host,host,...  set target node list on command line\n\
-e                use ssh to connect\n"

#define OPT_USAGE_SDR "\
-G                for -a, include all partitions of SP System\n\
-v                for -a, skip node if host_responds is false\n"

#define OPT_USAGE_KRB4 "\
-k                connect using kerberos IV (default for root)\n\
-R                connect using regular rcmd (default for non-root)\n"

#define OPT_USAGE_GEND "\
-g attribute      target nodes with specified genders attribute\n"

#define DSH_ARGS	"csS"
#define PCP_ARGS	"pr"
#define GEN_ARGS	"at:csqf:w:l:u:bI:ideV"
#define KRB4_ARGS	"Rk"
#define SDR_ARGS	"Gv"
#define GEND_ARGS	"g:"

/*
 * Set defaults for various options.
 *	opt (IN/OUT)	option struct
 */
void opt_default(opt_t *opt)
{
	struct passwd *pw;

	if (pw = getpwuid(getuid())) {
		strncpy(opt->luser, pw->pw_name, MAX_USERNAME);
		strncpy(opt->ruser, pw->pw_name, MAX_USERNAME);	
		opt->luid = pw->pw_uid;
        } else
                errx("%p: who are you?\n");

#if KRB4
	/* for Kerberos IV (probably SP), assume root can K4, users cannot */
	if (opt->luid == 0)
		opt->rcmd_type = RCMD_K4
	else
		opt->rcmd_type = RCMD_BSD
#else
	opt->rcmd_type = RCMD_BSD;
#endif

	opt->info_only = false;
	opt->wcoll = NULL;
	opt->progname = NULL;
	opt->connect_timeout = CONNECT_TIMEOUT;
	opt->command_timeout = 0;
	opt->fanout = DFLT_FANOUT;
	opt->sigint_terminates = false;
	opt->infile_names = list_new();
	opt->allnodes = false;
	opt->altnames = false;
	opt->debug = false;

	/* SDR */
	opt->sdr_verify = false;
	opt->sdr_global = false;

	/* DSH specific */
	opt->dshpath = NULL;
	opt->getstat = NULL;
	opt->cmd = NULL;
	opt->stdin_unavailable = false;
	opt->delete_nextpass = true;
	opt->separate_stderr = false;
	*(opt->gend_attr) = '\0';

	/* PCP specific */
	opt->outfile_name = NULL;
	opt->recursive = false;
	opt->preserve = false;

}

/*
 * Override default options with environment variables.
 *	opt (IN/OUT)	option struct	
 */
void opt_env(opt_t *opt)
{
	char *dshpath, *rhs;
	int dshpath_size;

	if (rhs = getenv("WCOLL"))
		opt->wcoll = read_wcoll(rhs, NULL);

        if (rhs = getenv("FANOUT"))
                opt->fanout = atoi(rhs);

        if (rhs = getenv("DSHPATH")) {
		struct passwd *pw = getpwnam(opt->luser);
		char *shell = "sh";

		if (pw && *pw->pw_shell)
			shell = xbasename(pw->pw_shell);
					/* c shell syntax */
		if (!strcmp(shell, "csh") || !strcmp(shell, "tcsh")) {
			opt->dshpath = xstrdup("setenv PATH ", &dshpath_size);
			xstrcat(&opt->dshpath, &dshpath_size, rhs);
			xstrcat(&opt->dshpath, &dshpath_size, ";");

		} else {		/* bourne shell syntax */
			opt->dshpath = xstrdup("PATH=", &dshpath_size);
			xstrcat(&opt->dshpath, &dshpath_size, rhs);
			xstrcat(&opt->dshpath, &dshpath_size, ";");
		}
        }
}

/*
 * Override  default/environment options with command line arguments.
 *	opt (IN/OUT)	option struct	
 * 	argc (IN)	arg count passed in from main
 * 	argv (IN)	arg vector passed in from main
 */
void opt_args(opt_t *opt, int argc, char *argv[])
{
	char validargs[LINEBUFSIZE];
	int c, cmd_size;
	extern int optind;
	extern char *optarg;
	char *pname = xbasename(argv[0]);

	/* deal with program name */
	opt->progname = pname;
	if (!strcmp(pname, "pdsh") || !strcmp(pname, "dsh"))
		opt->personality = DSH;
	else if (!strcmp(pname, "pdcp") || !strcmp(pname, "dcp") 
	    || !strcmp(pname, "pcp"))
		opt->personality = PCP;
	else
		errx("%p: program must be named pdsh/dsh/pdcp/dcp/pcp\n");

	/* construct valid arg list */
	if (opt->personality == DSH)
		strcpy(validargs, DSH_ARGS);
	else
		strcpy(validargs, PCP_ARGS);
	strcat(validargs, GEN_ARGS);
#if KRB4
 	strcat(validargs, KRB4_ARGS);
#endif
#if HAVE_SDRGETOBJECTS
 	strcat(validargs, SDR_ARGS);
#endif
#if HAVE_GENDERS
	strcat(validargs, GEND_ARGS);
#endif
#ifdef __linux
	/* Tell glibc getopt to stop eating after the first non-option arg */
	putenv("POSIXLY_CORRECT=1");
#endif
	while ((c = getopt(argc, argv, validargs)) != EOF) {
		switch (c) {
			case 'S':	/* get remote command status */
				opt->getstat = ";echo " RC_MAGIC "$?";
				break;
			case 'd':	/* debug */
				opt->debug = true;
				break;
			case 'f':	/* fanout */
				opt->fanout = atoi(optarg);
				break;
			case 'w':	/* target node list */
				if (strcmp(optarg, "-") == 0)
					opt->wcoll = read_wcoll(NULL, stdin);
				else 
					opt->wcoll = list_split(",", optarg);
				break;
			case 'g':	/* genders attribute */
				strncpy(opt->gend_attr, optarg, MAX_GENDATTR);
				break;
			case 'q':	/* display fanout and wcoll then quit */
				opt->info_only = true;
				break;
			case 's':	/* split stderr and stdout */
				opt->separate_stderr = true;
				break;
			case 'k':	/* use k4cmd() */
				opt->rcmd_type = RCMD_K4;
				break;
			case 'R':	/* use rcmd() */
				opt->rcmd_type = RCMD_BSD;
				break;
			case 'e':	/* use ssh */
				opt->rcmd_type = RCMD_SSH;
				break;
			case 'a':	/* indicates all nodes */
				opt->allnodes = true;
				break;
			case 'G':	/* pass 'Global' opt to SDRGetObjects */
				opt->sdr_global = true;
				break;
			case 'i':	/* use alternate hostnames */
				opt->altnames = true;
				break;
			case 'c':	/* continue to use hosts which have */
				opt->delete_nextpass = false;
				break;
			case 't':	/* set connect timeout */
				opt->connect_timeout = atoi(optarg);
				break;
			case 'u':	/* set command timeout */
				opt->command_timeout = atoi(optarg);
				break;
			case 'v':	/* verify hosts */
				opt->sdr_verify = true;
				break;
			case 'b':	/* "batch" */
				opt->sigint_terminates = true;
				break;
			case 'l':	/* specify remote username for rshd */
				strncpy(opt->ruser, optarg, MAX_USERNAME);
				break;
			case 'r':
				opt->recursive = true;
				break;
			case 'p':
				opt->preserve = true;
				break;
			case 'V':
				show_version();
				break;
			case 'h':	/* display usage message */
			default:
				usage(opt);
		}
	}

	/* DSH: build command */
	if (opt->personality == DSH) {
		for ( ; optind < argc; optind++) {
			if (opt->cmd != NULL)
				xstrcat(&opt->cmd, &cmd_size, " ");
			xstrcat(&opt->cmd, &cmd_size, argv[optind]);
		}
	
	/* PCP: build file list */
	} else {
		for ( ; optind < argc - 1; optind++)
			list_push(opt->infile_names, argv[optind]);
		if (optind < argc)
			xstrcat(&opt->outfile_name, &cmd_size, argv[optind]);
	}

	/* get wcoll, SDR, genders file, or MPICH machines file */
	if (opt->allnodes) {
#if HAVE_SDRGETOBJECTS
		opt->wcoll = sdr_wcoll(opt->sdr_global, 
		    opt->altnames, opt->sdr_verify);
#elif HAVE_GENDERS
		opt->wcoll = read_genders("all", opt->altnames);
#else
		opt->wcoll = read_wcoll(_PATH_MACHINES, NULL);
#endif
	} 
#if HAVE_GENDERS
	if (*(opt->gend_attr)) {
		opt->wcoll = read_genders(opt->gend_attr, opt->altnames);
	}
#endif
}

/*
 * Trap a few option inconsitencies.
 *	opt (IN)	option struct
 */
bool opt_verify(opt_t *opt)
{
	bool verified = true;
	int i;

	/* can't prompt for command if stdin was used for wcoll */
	if (opt->personality == DSH && opt->stdin_unavailable && !opt->cmd) {
		usage(opt);
		verified = false;
	}

	/* wcoll is required */
	if (opt->wcoll == NULL || list_length(opt->wcoll) == 0) {
		err("%p: no remote hosts specified\n");
		verified = false;
	}

	/* connect and command timeouts must be reasonable */
	if (opt->connect_timeout < 0) {
		err("%p: connect timeout must be > 0\n");
		verified = false;
	}
	if (opt->command_timeout < 0) {
		err("%p: command timeout must be > 0\n");
		verified = false;
	}

	/* When using ssh, connect timeout is out of our juristiction */
	if (opt->rcmd_type == RCMD_SSH) {
		if (opt->connect_timeout != CONNECT_TIMEOUT) {
			err("%p: -e and -t are incompatible\n");
			verified = false;
		}
	}

	/* PCP: must have source and destination filename(s) */
	if (opt->personality == PCP) {
		if (!opt->outfile_name || list_length(opt->infile_names) == 0) {
			err("%p: pcp requires source and dest filenames\n");
			verified = false;
		}
	}

	/* verify infile(s) */
	for (i = 0; i < list_length(opt->infile_names); i++) {
		struct stat sb;
		char *name = list_nth(opt->infile_names, i);

		if (stat(name, &sb) < 0) {
			err("%p: can't stat %s\n", name);
			verified = false;
			continue;
		}
		if (!S_ISREG(sb.st_mode) && !S_ISDIR(sb.st_mode)) {
			err("%p: not a regular file or directory: %s\n", name);
			verified = false;
			break;
		}
		if (S_ISDIR(sb.st_mode) && !opt->recursive) {
			err("%p: use -r to copy directories: %s\n", name);
			verified = false;
			break;
		}
	}		

	return verified;
}

/* printf args */
#define BOOLSTR(x)	((x) ? "Yes" : "No")
#define STRORNULL(x)	((x) ? (x) : "none")
#define RCMDSTR(x)	(x == RCMD_BSD ? "RCMD_BSD" :  \
			    (x == RCMD_K4 ? "RCMD_K4" : \
			         (x == RCMD_SSH ? "RCMD_SSH" : "<Unknown>")))

/*
 * List the current options.
 *	opt (IN)	option list
 */
void opt_list(opt_t *opt)
{
	char *wcoll_str, *infile_str;

	if (opt->personality == DSH) {
	out("-- DSH-specific options --\n");
	out("Separate stderr/stdout	%s\n", BOOLSTR(opt->separate_stderr));
	out("Delete on next pass	%s\n", BOOLSTR(opt->delete_nextpass));
	out("Path prepended to cmd	%s\n", STRORNULL(opt->dshpath));
	out("Appended to cmd         %s\n", STRORNULL(opt->getstat));
	out("Command:		%s\n", STRORNULL(opt->cmd));
	} else {
	out("-- PCP-specific options --\n");
	out("Outfile			%s\n", STRORNULL(opt->outfile_name));
	out("Recursive		%s\n", BOOLSTR(opt->recursive));
	out("Preserve mod time/mode	%s\n", BOOLSTR(opt->preserve));
	}
	out("\n-- Generic options --\n");
	out("Local username		%s\n", opt->luser);
	out("Local uid     		%d\n", opt->luid);
	out("Remote username		%s\n", opt->ruser);
	out("Rcmd type		%s\n", RCMDSTR(opt->rcmd_type));
	out("one ^C will kill pdsh   %s\n", BOOLSTR(opt->sigint_terminates));
	out("Connect timeout (secs)	%d\n", opt->connect_timeout);
	out("Command timeout (secs)	%d\n", opt->command_timeout);
	out("Fanout			%d\n", opt->fanout);
	out("All nodes       	%s\n", BOOLSTR(opt->allnodes));
	infile_str = list_join(", ", opt->infile_names);
	if (infile_str) {
		out("Infile(s)		%s\n", infile_str);
		xfree(&infile_str);
	}
	out("Use alt hostname	%s\n", BOOLSTR(opt->altnames));
	out("Debugging       	%s\n", BOOLSTR(opt->debug));

	out("\n-- SDR options --\n");
	out("Verify SDR nodes  	%s\n", BOOLSTR(opt->sdr_verify));
	out("All SDR partitions	%s\n", BOOLSTR(opt->sdr_global));

	out("\n-- Target nodes --\n");
	wcoll_str = list_join(",", opt->wcoll);
	out("%s\n", wcoll_str);

	xfree(&wcoll_str);
}

/*
 * Free heap-allocated memory associated with options, etc.
 *	opt (IN/OUT)	option struct
 */
void opt_free(opt_t *opt)
{
	if (opt->wcoll != NULL)
		list_free(&opt->wcoll);
	if (opt->cmd != NULL)
		xfree(&opt->cmd);		
}

/*
 * Spit out all the options and their one-line synopsis for the user, 
 * then exit.
 */
static void usage(opt_t *opt)
{
	if (opt->personality == DSH)
		err(OPT_USAGE_DSH);
	else /* PCP */
		err(OPT_USAGE_PCP);
	err(OPT_USAGE_COMMON);
#if KRB4
	err(OPT_USAGE_KRB4);
#endif
#if HAVE_SDRGETOBJECTS
	err(OPT_USAGE_SDR);
#endif
#if HAVE_GENDERS
	err(OPT_USAGE_GEND);
#endif
	exit(1);
}

static void show_version(void)
{
	printf("pdsh version %s\n", PDSH_VERSION);
	exit(0);
}

/* 
 * $Id$ 
 * 
 * Copyright (C) 2000-2002 Regents of the University of California
 * See ./DISCLAIMER
 */

#if     HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>	/* strcpy */
#include <stdlib.h>	/* getenv */
#include <pwd.h>	/* getpwuid */

#include <sys/types.h>
#include <sys/stat.h>	/* stat */
#if	HAVE_UNISTD_H
#include <unistd.h>	/* getopt */
#endif

#include "dsh.h"
#include "opt.h"
#include "err.h"
#include "list.h"
#include "wcoll.h"
#include "xstring.h"
#include "xmalloc.h"	

static void usage(opt_t *opt);
static void show_version(void);

#define OPT_USAGE_DSH "\
Usage: pdsh [-options] command ...\n\
-S                return largest of remote command return values\n"

#if	HAVE_MAGIC_RSHELL_CLEANUP
#define OPT_USAGE_STDERR "\
-s                separate stderr and stdout\n"
#else
#define OPT_USAGE_STDERR "\
-s                combine stderr with stdout to conserve sockets\n"
#endif


#define OPT_USAGE_PCP "\
Usage: pdcp [-options] src [src2...] dest\n\
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
-x                do not expand hostname ranges from -w\n\
-X c              set hostname range expansion operator (default '-')\n"

#define OPT_USAGE_SDR "\
-G                for -a, include all partitions of SP System\n\
-v                for -a, skip node if host_responds is false\n"

#define OPT_USAGE_GEND "\
-g attribute      target nodes with specified genders attribute\n"

#define OPT_USAGE_ELAN "\
-E                run Quadrics Elan job using qshell\n\
-m block|cyclic   (qshell) control assignment of procs to nodes\n\
-P partition      (qshell) alloc nodes from RMS (req -n and/or -N)\n\
-N n              (qshell) set total number of nodes for job (req -P)\n\
-n n              (qshell) set total number of cpus for job\n"

#define DSH_ARGS	"csS"
#define PCP_ARGS	"pr"
#define GEN_ARGS	"at:csqf:w:xX:l:u:bI:ideV"
#define SDR_ARGS	"Gv"
#define GEND_ARGS	"g:"
#define ELAN_ARGS	"En:m:P:N:"

/*
 * Set defaults for various options.
 *	opt (IN/OUT)	option struct
 */
void 
opt_default(opt_t *opt)
{
	struct passwd *pw;

	if ((pw = getpwuid(getuid())) != NULL) {
		strncpy(opt->luser, pw->pw_name, MAX_USERNAME);
		strncpy(opt->ruser, pw->pw_name, MAX_USERNAME);	
		opt->luid = pw->pw_uid;
        } else
                errx("%p: who are you?\n");

	/* set the default connect method */
#if 	HAVE_SSH
	opt->rcmd_type = RCMD_SSH;
#elif 	HAVE_KRB4
	opt->rcmd_type = RCMD_K4;
#else
	opt->rcmd_type = RCMD_BSD;
#endif

	opt->info_only = false;
	opt->wcoll = NULL;
	opt->range_op = RANGE_OP;
	opt->progname = NULL;
	opt->connect_timeout = CONNECT_TIMEOUT;
	opt->command_timeout = 0;
	opt->fanout = DFLT_FANOUT;
	opt->sigint_terminates = false;
	opt->infile_names = list_new();
	opt->allnodes = false;
	opt->altnames = false;
	opt->debug = false;
	opt->labels = true;

	/* SDR */
	opt->sdr_verify = false;
	opt->sdr_global = false;

	/* DSH specific */
	opt->dshpath = NULL;
	opt->getstat = NULL;
	opt->cmd = NULL;
	opt->stdin_unavailable = false;
	opt->delete_nextpass = true;
#if	HAVE_MAGIC_RSHELL_CLEANUP
	opt->separate_stderr = false; /* save a socket per connection on aix */
#else
	opt->separate_stderr = true;
#endif
	*(opt->gend_attr) = '\0';
	opt->q_nprocs = -1;
	opt->q_allocation = ALLOC_UNSPEC;
	opt->rms_partition = NULL;
	opt->rms_nnodes = -1;

	/* PCP specific */
	opt->outfile_name = NULL;
	opt->recursive = false;
	opt->preserve = false;
}

/*
 * Override default options with environment variables.
 *	opt (IN/OUT)	option struct	
 */
void 
opt_env(opt_t *opt)
{
	char *rhs;

	if ((rhs = getenv("WCOLL")) != NULL)
		opt->wcoll = read_wcoll(rhs, NULL);

        if ((rhs = getenv("FANOUT")) != NULL)
                opt->fanout = atoi(rhs);

	if ((rhs = getenv("PDSH_RANGE_OPERATOR")) != NULL)
		opt->range_op = (strlen(rhs) > 0) ? Strdup(rhs) : NULL;

        if ((rhs = getenv("DSHPATH")) != NULL) {
		struct passwd *pw = getpwnam(opt->luser);
		char *shell = "sh";

		if (pw && *pw->pw_shell)
			shell = xbasename(pw->pw_shell);
					/* c shell syntax */
		if (!strcmp(shell, "csh") || !strcmp(shell, "tcsh")) {
			opt->dshpath = Strdup("setenv PATH ");
			xstrcat(&opt->dshpath, rhs);
			xstrcat(&opt->dshpath, ";");

		} else {		/* bourne shell syntax */
			opt->dshpath = Strdup("PATH=");
			xstrcat(&opt->dshpath, rhs);
			xstrcat(&opt->dshpath, ";");
		}
        }
}

/*
 * Override  default/environment options with command line arguments.
 *	opt (IN/OUT)	option struct	
 * 	argc (IN)	arg count passed in from main
 * 	argv (IN)	arg vector passed in from main
 */
void 
opt_args(opt_t *opt, int argc, char *argv[])
{
	char validargs[LINEBUFSIZE];
	int c;
	extern int optind;
	extern char *optarg;
	char *wcoll_buf;
	char *pname = xbasename(argv[0]);

	wcoll_buf = NULL;

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
	if (opt->personality == DSH) {
		strcpy(validargs, DSH_ARGS);
#if 	HAVE_ELAN3
		strcat(validargs, ELAN_ARGS);
#endif
	} else
		strcpy(validargs, PCP_ARGS);
	strcat(validargs, GEN_ARGS);
#if	HAVE_SDR
 	strcat(validargs, SDR_ARGS);
#endif
#if	HAVE_GENDERS
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
				        wcoll_buf = Strdup(optarg);
				break;
			case 'x':       /* no ranges */
				opt->range_op = NULL;
				break;
			case 'X':
				if (strlen(optarg) == 1)
					opt->range_op = Strdup(optarg);
				else
					usage(opt);
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
			case 'E':	/* use qshell */
				opt->rcmd_type = RCMD_QSHELL;
				break;
			case 'n':	/* set number of procs (qshell) */
				opt->q_nprocs = atoi(optarg);
				break;
			case 'm':	/* set block or cyclic allocation (qshell) */
				if (strcmp(optarg, "block") == 0)
					opt->q_allocation = ALLOC_BLOCK;
				else if (strcmp(optarg, "cyclic") == 0)
					opt->q_allocation = ALLOC_CYCLIC;
				else
					usage(opt);
				break;
			case 'N':	/* set number of nodes (qshell) */
				opt->rms_nnodes = atoi(optarg);
				break;
			case 'P':	/* allocate nodes from RMS partition (qshell) */
				opt->rms_partition = Strdup(optarg);
				break;
			case 'a':	/* indicates all nodes */
				opt->allnodes = true;
				break;
			case 'G':	/* pass 'Global' opt to SDRGetObjects */
				opt->sdr_global = true;
				break;
			case 'i':	/* use alternate hostnames */
				opt->altnames = true;
#if	HAVE_MACHINES
				err("%p: warning: -i will have no effect\n");
#endif
				break;
			case 'c':	/* pdsh> continue to try failed hosts */
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

	/* expand wcoll if needed */
	if (wcoll_buf != NULL) {
		opt->wcoll = (opt->range_op == NULL) ? 
			list_split(",", wcoll_buf) :
			list_split_range(",", opt->range_op, wcoll_buf);
		Free((void **)&wcoll_buf);
	}

	/* DSH: build command */
	if (opt->personality == DSH) {
		for ( ; optind < argc; optind++) {
			if (opt->cmd != NULL)
				xstrcat(&opt->cmd, " ");
			xstrcat(&opt->cmd, argv[optind]);
		}
	
	/* PCP: build file list */
	} else {
		for ( ; optind < argc - 1; optind++)
			list_push(opt->infile_names, argv[optind]);
		if (optind < argc)
			xstrcat(&opt->outfile_name, argv[optind]);
	}

	/* get wcoll, SDR, genders file, or MPICH machines file */
	if (opt->allnodes) {
#if	HAVE_MACHINES
		opt->wcoll = read_wcoll(_PATH_MACHINES, NULL);
#elif 	HAVE_SDR
		opt->wcoll = sdr_wcoll(opt->sdr_global, 
		    opt->altnames, opt->sdr_verify);
#elif 	HAVE_GENDERS
		opt->wcoll = read_genders("all", opt->altnames);
#else
#error bogus configuration
#endif
	} 
#ifdef	HAVE_GENDERS
	/* get wcoll from genders - all nodes with a particular attribute */
	if (*(opt->gend_attr)) {
		opt->wcoll = read_genders(opt->gend_attr, opt->altnames);
	}
#endif
#if 	HAVE_RMS_PMANAGER
	/* get wcoll from RMS partition manager */
	if (opt->rms_partition || opt->wcoll == NULL) { 
		/* catch couple of errors early */
		if (opt->wcoll)
			errx("%p: RMS cannot be used with -w or other lists\n");
		if (opt->rms_nnodes == -1 && opt->q_nprocs == -1)
			errx("%p: RMS requires -N and/or -n\n");
		if (opt->q_nprocs != -1 && opt->rcmd_type != RCMD_QSHELL)
			errx("%p: -n requires -E\n");

		if (opt->q_nprocs == -1)
			opt->q_nprocs = opt->rms_nnodes;
		if (opt->rms_nnodes == -1)
			opt->rms_nnodes = opt->q_nprocs;

		opt->wcoll = rms_wcoll(opt->rms_partition, opt->rms_nnodes, 
					opt->q_nprocs);
	}
#endif /* HAVE_RMS_PMANAGER */
#if 	HAVE_ELAN3
	if (opt->rcmd_type == RCMD_QSHELL) {
		if (opt->fanout == DFLT_FANOUT && opt->wcoll != NULL)
			opt->fanout = list_length(opt->wcoll);
		if (opt->q_allocation == ALLOC_UNSPEC)
			opt->q_allocation = ALLOC_BLOCK;
		opt->labels = false;
		if (opt->dshpath != NULL)
			Free((void **)&opt->dshpath);
	}
#endif /* HAVE_ELAN3 */
	if (opt->q_nprocs == -1 && opt->wcoll != NULL)
		opt->q_nprocs = list_length(opt->wcoll);
}

/*
 * Trap a few option inconsitencies.
 *	opt (IN)	option struct
 */
bool 
opt_verify(opt_t *opt)
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
		err("%p: connect timeout must be >= 0\n");
		verified = false;
	}
	if (opt->command_timeout < 0) {
		err("%p: command timeout must be >= 0\n");
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

	/* Constraints when running Elan jobs */
	if (opt->rcmd_type == RCMD_QSHELL) {
		if (opt->wcoll != NULL) {
			if (opt->fanout != list_length(opt->wcoll)) {
				err("%p: fanout must = target node list length with -E\n");
				verified = false;
			}
			if (opt->q_nprocs % list_length(opt->wcoll) != 0) {
				err("%p: -n argument must be multiple of target node list length\n");
				verified = false;
			}
		}
		if (opt->q_nprocs <= 0) {
			err("%p: -n option should be >= 1\n");
			verified = false;
		}
	} else {
		if (opt->wcoll != NULL) {
			if (opt->q_nprocs != list_length(opt->wcoll)) {
				err("%p: -n can only be specified with -E\n");
				verified = false;
			}
		}
		if (opt->q_allocation != ALLOC_UNSPEC) {
			err("%p: -m can only be specified with -E\n");
			verified = false;
		}
	}

	return verified;
}

/* printf args */
#define BOOLSTR(x)	((x) ? "Yes" : "No")
#define STRORNULL(x)	((x) ? (x) : "none")
#define RCMDSTR(x)	(x == RCMD_BSD ? "RCMD_BSD" :  \
			  (x == RCMD_K4 ? "RCMD_K4" : \
			    (x == RCMD_QSHELL ? "RCMD_QSHELL" : \
			      (x == RCMD_SSH ? "RCMD_SSH" : "<Unknown>"))))
#define ALLOCSTR(x)	(x == ALLOC_BLOCK ? "ALLOC_BLOCK" : \
			  (x == ALLOC_CYCLIC ? "ALLOC_CYCLIC" : "<Unknown>"))

/*
 * List the current options.
 *	opt (IN)	option list
 */
void 
opt_list(opt_t *opt)
{
	char *wcoll_str, *infile_str;

	if (opt->personality == DSH) {
	out("-- DSH-specific options --\n");
	out("Separate stderr/stdout	%s\n", BOOLSTR(opt->separate_stderr));
	out("Delete on next pass	%s\n", BOOLSTR(opt->delete_nextpass));
	out("(elan) nprocs       	%d\n", opt->q_nprocs);
	out("(elan) allocation     	%s\n", ALLOCSTR(opt->q_allocation));
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
	out("Display hostname labels	%s\n", BOOLSTR(opt->labels));
	out("All nodes       	%s\n", BOOLSTR(opt->allnodes));
	infile_str = list_join(", ", opt->infile_names);
	if (infile_str) {
		out("Infile(s)		%s\n", infile_str);
		Free((void **)&infile_str);
	}
	out("Use alt hostname	%s\n", BOOLSTR(opt->altnames));
	out("Range operator		`%s'\n", 
			opt->range_op ? opt->range_op : "NULL");
	out("Debugging       	%s\n", BOOLSTR(opt->debug));

	out("\n-- SDR options --\n");
	out("Verify SDR nodes  	%s\n", BOOLSTR(opt->sdr_verify));
	out("All SDR partitions	%s\n", BOOLSTR(opt->sdr_global));

	out("\n-- Target nodes --\n");
	wcoll_str = list_join(",", opt->wcoll);
	out("%s\n", wcoll_str);

	Free((void **)&wcoll_str);
}

/*
 * Free heap-allocated memory associated with options, etc.
 *	opt (IN/OUT)	option struct
 */
void 
opt_free(opt_t *opt)
{
	if (opt->wcoll != NULL)
		list_free(&opt->wcoll);
	if (opt->cmd != NULL)
		Free((void **)&opt->cmd);		
}

/*
 * Spit out all the options and their one-line synopsis for the user, 
 * then exit.
 */
static void 
usage(opt_t *opt)
{
	if (opt->personality == DSH) {
		err(OPT_USAGE_DSH);
		err(OPT_USAGE_STDERR);
#if 	HAVE_ELAN3
		err(OPT_USAGE_ELAN);
#endif
	} else /* PCP */
		err(OPT_USAGE_PCP);
	err(OPT_USAGE_COMMON);
#if 	HAVE_KRB4
	err(OPT_USAGE_KRB4);
#endif
#if	HAVE_SDR
	err(OPT_USAGE_SDR);
#endif
#if	HAVE_GENDERS
	err(OPT_USAGE_GEND);
#endif
	exit(1);
}

static void show_version(void)
{
	printf("%s-%s (", PROJECT, VERSION);
#ifdef	HAVE_SDR
	printf("+sdr");
#endif
#ifdef	HAVE_GENDERS
	printf("+genders");
#endif
#ifdef 	HAVE_MACHINES
	printf("+machines");
#endif
#if	HAVE_ELAN3
	printf("+elan");
#endif
#if	HAVE_RMS_PMANAGER
	printf("+rms");
#endif
#if	HAVE_SSH
	printf("+ssh");
#endif
#if	HAVE_KRB4
	printf("+krb4");
#endif
#if	!NDEBUG
	printf("+debug");
#endif
#if 	WITH_DMALLOC
	printf("+dmalloc");
#endif
	printf(")\n");
	exit(0);
}

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

#if HAVE_STRING_H
#include <string.h>             /* strcpy */
#endif
#include <stdlib.h>             /* getenv */
#include <pwd.h>                /* getpwuid */

#include <sys/types.h>
#include <sys/stat.h>           /* stat */
#if	HAVE_UNISTD_H
#include <unistd.h>             /* getopt */
#endif

#include "dsh.h"
#include "hostlist.h"
#include "opt.h"
#include "err.h"
#include "list.h"
#include "wcoll.h"
#include "xstring.h"
#include "xmalloc.h"

#include "mod.h"
#include "mod_rcmd.h"

static void _usage(opt_t * opt);
static void _show_version(void);

#define OPT_USAGE_DSH "\
Usage: pdsh [-options] command ...\n\
-S                return largest of remote command return values\n"

/* -s option only useful on AIX */
#if	HAVE_MAGIC_RSHELL_CLEANUP
#define OPT_USAGE_STDERR "\
-s                separate stderr and stdout\n"
#endif


#define OPT_USAGE_PCP "\
Usage: pdcp [-options] src [src2...] dest\n\
-r                recursively copy files\n\
-p                preserve modification time and modes\n"

#define OPT_USAGE_COMMON "\
-h                output usage menu and quit\n\
-q                list the option settings and quit\n\
-b                disable ^C status feature (batch mode)\n\
-l user           execute remote commands as user\n\
-t seconds        set connect timeout (default is 10 sec)\n\
-u seconds        set command timeout (no default)\n\
-f n              use fanout of n nodes\n\
-w host,host,...  set target node list on command line\n\
-x host,host,...  set node exclusion list on command line\n\
-R name           set rcmd module to name\n\
-L                list info on all loaded modules and exit\n"
/* undocumented "-T testcase" option */
/* undocumented "-Q" option */


#if	HAVE_MAGIC_RSHELL_CLEANUP
#define DSH_ARGS	"sS"
#else
#define DSH_ARGS        "S"
#endif
#define PCP_ARGS	"pr"
#define GEN_ARGS	"hLR:t:cqf:w:x:l:u:bI:deVT:Q"

/*
 *  Pdsh options string (for getopt) -- initialized
 *    in _init_pdsh_options(), and appended to by modules that
 *    register new pdsh options.
 */    
static char *pdsh_options = NULL;

static void
_init_pdsh_options(void)
{
	pdsh_options = Strdup(GEN_ARGS);
	xstrcat(&pdsh_options, DSH_ARGS);
	xstrcat(&pdsh_options, PCP_ARGS);
}

/*
 *  Check whether option `opt' is available for provisioning
 *    and register the option if so.
 *
 *  Returns false if option is already used by pdsh or a pdsh module.
 *  Returns true if option was successfully registered.
 */
bool opt_register(struct pdsh_module_option *p)
{
	if (pdsh_options == NULL)
		_init_pdsh_options();

	if (!strchr(pdsh_options, p->opt)) {
		xstrcatchar(&pdsh_options, p->opt);
		if (p->arginfo != NULL)
			xstrcatchar(&pdsh_options, ':');
		return true;
	}

	return false;
}


/*
 * Set defaults for various options.
 *	opt (IN/OUT)	option struct
 */
void opt_default(opt_t * opt)
{
    struct passwd *pw;

    if (pdsh_options == NULL)
	    _init_pdsh_options();

    if ((pw = getpwuid(getuid())) != NULL) {
        strncpy(opt->luser, pw->pw_name, MAX_USERNAME);
        strncpy(opt->ruser, pw->pw_name, MAX_USERNAME);
        opt->luid = pw->pw_uid;
    } else
        errx("%p: who are you?\n");

    opt->info_only = false;
    opt->test_range_expansion = false;
    opt->wcoll = NULL;
    opt->progname = NULL;
    opt->connect_timeout = CONNECT_TIMEOUT;
    opt->command_timeout = 0;
    opt->fanout = DFLT_FANOUT;
    opt->sigint_terminates = false;
    opt->infile_names = list_create(NULL);
    opt->altnames = false;
    opt->debug = false;
    opt->labels = true;

    opt->rcmd_name = NULL;

    /*
     *  Resolve hostnames by default
     */
    opt->resolve_hosts = true; 

    /*
     *  Do not kill all tasks on single failure by default
     */
    opt->kill_on_fail = false;

    /* DSH specific */
    opt->dshpath = NULL;
    opt->getstat = NULL;
    opt->cmd = NULL;
    opt->stdin_unavailable = false;
#if	HAVE_MAGIC_RSHELL_CLEANUP
    opt->separate_stderr = false;       /* save a socket per connection on aix */
#else
    opt->separate_stderr = true;
#endif

    /* PCP specific */
    opt->outfile_name = NULL;
    opt->recursive = false;
    opt->preserve = false;

}

/*
 * Override default options with environment variables.
 *	opt (IN/OUT)	option struct	
 */
void opt_env(opt_t * opt)
{
    char *rhs;

    if ((rhs = getenv("WCOLL")) != NULL)
        opt->wcoll = read_wcoll(rhs, NULL);

    if ((rhs = getenv("FANOUT")) != NULL)
        opt->fanout = atoi(rhs);

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

        } else {                /* bourne shell syntax */
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
void opt_args(opt_t * opt, int argc, char *argv[])
{
    char *validargs = NULL;
    int c;
    extern int optind;
    extern char *optarg;
    char *exclude_buf = NULL;
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
    if (opt->personality == DSH) {
        xstrcpy(&validargs, DSH_ARGS);
    } else
        xstrcpy(&validargs, PCP_ARGS);
    xstrcat(&validargs, GEN_ARGS);

#ifdef __linux
    /* Tell glibc getopt to stop eating after the first non-option arg */
    putenv("POSIXLY_CORRECT=1");
#endif
    while ((c = getopt(argc, argv, pdsh_options)) != EOF) {
        switch (c) {
        case 'L':
            mod_list_module_info();
            exit(0);
            break;
        case 'R': 
            opt->rcmd_name = Strdup(optarg);
            break;
        case 'S':              /* get remote command status */
            opt->getstat = ";echo " RC_MAGIC "$?";
            break;
        case 'd':              /* debug */
            opt->debug = true;
            break;
        case 'f':              /* fanout */
            opt->fanout = atoi(optarg);
            break;
        case 'w':              /* target node list */
            if (strcmp(optarg, "-") == 0)
                opt->wcoll = read_wcoll(NULL, stdin);
            else
                opt->wcoll = hostlist_create(optarg);
            break;
        case 'x':              /* exclude node list */
            exclude_buf = Strdup(optarg);
            break;
        case 'q':              /* display fanout and wcoll then quit */
            opt->info_only = true;
            break;
/* -s option only useful on AIX */
#if	HAVE_MAGIC_RSHELL_CLEANUP
        case 's':              /* split stderr and stdout */
            opt->separate_stderr = true;
            break;
#endif
        case 't':              /* set connect timeout */
            opt->connect_timeout = atoi(optarg);
            break;
        case 'u':              /* set command timeout */
            opt->command_timeout = atoi(optarg);
            break;
        case 'b':              /* "batch" */
            opt->sigint_terminates = true;
            break;
        case 'l':              /* specify remote username for rshd */
            strncpy(opt->ruser, optarg, MAX_USERNAME);
            break;
        case 'r':              /* rcp: copy recursively */
            opt->recursive = true;
            break;
        case 'p':              /* rcp: preserve permissions */
            opt->preserve = true;
            break;
        case 'V':              /* show version */
            _show_version();
            break;
        case 'T':              /* execute testcase */
            testcase(atoi(optarg));
            break;
        case 'Q':              /* info only, expand host ranges */
            opt->info_only = true;
            opt->test_range_expansion = true;
            break;
        case 'h':              /* display usage message */
            _usage(opt);
            break;
        default:
            if (mod_process_opt(opt, c, optarg) < 0)
               _usage(opt);
        }
    }

    Free((void **) &validargs);

    /*
     *  Load requested (or default) rcmd module 
     */
    if (mod_rcmd_load(opt) < 0)
        exit(1);

    /* DSH: build command */
    if (opt->personality == DSH) {
        for (; optind < argc; optind++) {
            if (opt->cmd != NULL)
                xstrcat(&opt->cmd, " ");
            xstrcat(&opt->cmd, argv[optind]);
        }

        /* PCP: build file list */
    } else {
        for (; optind < argc - 1; optind++)
            list_append(opt->infile_names, argv[optind]);
        if (optind < argc)
            xstrcat(&opt->outfile_name, argv[optind]);
    }

    /*
     *  Attempt to grab wcoll from modules stack unless
     *    wcoll was set from -w.
     */
    if (mod_read_wcoll(opt) < 0)
        exit(1);

    /* handle -x option */
    if (exclude_buf != NULL && opt->wcoll) {
        if (hostlist_delete(opt->wcoll, exclude_buf) == 0) {
            errx("%p: Invalid argument to -x: `%s'\n", exclude_buf);
            exit(1);
        }
        Free((void **) &exclude_buf);
    }

}

/*
 * Trap a few option inconsitencies.
 *	opt (IN)	option struct
 */
bool opt_verify(opt_t * opt)
{
    bool verified = true;
    ListIterator i;
    char *name;

    /*
     *  Call all post option processing functions for modules
     */
    if (mod_postop(opt) > 0)
        verified = false;

    /* can't prompt for command if stdin was used for wcoll */
    if (opt->personality == DSH && opt->stdin_unavailable && !opt->cmd) {
        _usage(opt);
        verified = false;
    }

    /* wcoll is required */
    if (opt->wcoll == NULL || hostlist_count(opt->wcoll) == 0) {
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

    /* PCP: must have source and destination filename(s) */
    if (opt->personality == PCP) {
        if (!opt->outfile_name || list_is_empty(opt->infile_names)) {
            err("%p: pcp requires source and dest filenames\n");
            verified = false;
        }
    }

    i = list_iterator_create(opt->infile_names);
    while ((name = list_next(i))) {
        struct stat sb;
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
    list_iterator_destroy(i);

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

static char * _list_join(const char *sep, List l)
{
    char *str = NULL;
    char *result = NULL;
    ListIterator i;
        
    if (list_count(l) == 0)
        return NULL;
        
    i = list_iterator_create(l);
    while ((str = list_next(i))) {
        char buf[1024];
        snprintf(buf, 1024, "%s%s", str, sep); 
        xstrcat(&result, buf);
    }
    list_iterator_destroy(i);

    /* 
     * Delete final separator
     */
    result[strlen(result) - strlen(sep)] = '\0';

    return result;
}

/*
 * List the current options.
 *	opt (IN)	option list
 */
void opt_list(opt_t * opt)
{
    char *infile_str;
    char wcoll_str[1024];
    int n;

    if (opt->personality == DSH) {
        out("-- DSH-specific options --\n");
        out("Separate stderr/stdout	%s\n",
            BOOLSTR(opt->separate_stderr));
        out("Path prepended to cmd	%s\n", STRORNULL(opt->dshpath));
        out("Appended to cmd         %s\n", STRORNULL(opt->getstat));
        out("Command:		%s\n", STRORNULL(opt->cmd));
    } else {
        out("-- PCP-specific options --\n");
        out("Outfile			%s\n",
            STRORNULL(opt->outfile_name));
        out("Recursive		%s\n", BOOLSTR(opt->recursive));
        out("Preserve mod time/mode	%s\n", BOOLSTR(opt->preserve));
    }
    out("\n-- Generic options --\n");
    out("Local username		%s\n", opt->luser);
    out("Local uid     		%d\n", opt->luid);
    out("Remote username		%s\n", opt->ruser);
    out("Rcmd type		%s\n", opt->rcmd_name);
    out("one ^C will kill pdsh   %s\n", BOOLSTR(opt->sigint_terminates));
    out("Connect timeout (secs)	%d\n", opt->connect_timeout);
    out("Command timeout (secs)	%d\n", opt->command_timeout);
    out("Fanout			%d\n", opt->fanout);
    out("Display hostname labels	%s\n", BOOLSTR(opt->labels));
    infile_str = _list_join(", ", opt->infile_names);
    if (infile_str) {
        out("Infile(s)		%s\n", infile_str);
        Free((void **) &infile_str);
    }
    out("Debugging       	%s\n", BOOLSTR(opt->debug));

    out("\n-- Target nodes --\n");
    if (opt->test_range_expansion) {
        n = hostlist_deranged_string(opt->wcoll, sizeof(wcoll_str),
                                     wcoll_str);
    } else {
        n = hostlist_ranged_string(opt->wcoll, sizeof(wcoll_str),
                                   wcoll_str);
    }

    if (n < 0)
        out("%s[truncated]\n", wcoll_str);
    else
        out("%s\n", wcoll_str);
}

/*
 * Free heap-allocated memory associated with options, etc.
 *	opt (IN/OUT)	option struct
 */
void opt_free(opt_t * opt)
{
    if (opt->wcoll != NULL)
        hostlist_destroy(opt->wcoll);
    if (opt->cmd != NULL)
        Free((void **) &opt->cmd);
    if (opt->rcmd_name != NULL)
        Free((void **) &opt->rcmd_name);
    if (pdsh_options)
        Free((void **) &pdsh_options);
    if (opt->dshpath)
        Free((void **) &opt->dshpath);
}

/*
 * Spit out all the options and their one-line synopsis for the user, 
 * then exit.
 */
static void _usage(opt_t * opt)
{
    List l      = NULL;
    char *names = NULL;
    char *def   = NULL;

    /* first, make sure atleast some rcmd modules are loaded */
    l = mod_get_module_names("rcmd");
    if (list_count(l) == 0)
      errx("%p: no rcmd modules are loaded\n");
    names = _list_join(",", l);
    list_destroy(l);

    if (!(def = mod_rcmd_get_default_module()))
        def = "(none)";

    if (opt->personality == DSH) {
        err(OPT_USAGE_DSH);
#if	HAVE_MAGIC_RSHELL_CLEANUP
        err(OPT_USAGE_STDERR);
#endif
    } else                      /* PCP */
        err(OPT_USAGE_PCP);

    err(OPT_USAGE_COMMON);

    mod_print_all_options(18);

    err("available rcmd modules: %s (default: %s)\n", names, def);
    Free((void **) &names);

    exit(1);
}


static void _show_version(void)
{
    extern char *pdsh_version;
    printf("%s (", pdsh_version);
#if	!NDEBUG
    printf("+debug");
#endif
#if 	WITH_DMALLOC
    printf("+dmalloc");
#endif
    printf(")\n");
    exit(0);
}


/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

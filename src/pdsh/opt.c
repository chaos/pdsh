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
#include <sys/param.h>          /* MAXPATHLEN */

#include <sys/types.h>
#include <sys/stat.h>           /* stat */
#if	HAVE_UNISTD_H
#include <unistd.h>             /* getopt */
#endif

#include <errno.h>

#include "src/common/hostlist.h"
#include "src/common/err.h"
#include "src/common/list.h"
#include "src/common/xstring.h"
#include "src/common/xmalloc.h"
#include "dsh.h"                
#include "opt.h"
#include "wcoll.h"
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
/* undocumented "-y"  target must be directory option */
/* undocumented "-z"  run pdcp server option */

#define OPT_USAGE_COMMON "\
-h                output usage menu and quit\n\
-V                output version information and quit\n\
-q                list the option settings and quit\n\
-b                disable ^C status feature (batch mode)\n\
-d                enable extra debug information from ^C status\n\
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
#define PCP_ARGS	"pryz"
#define GEN_ARGS	"hLR:t:cqf:w:x:l:u:bI:deVT:Q"

/*
 *  Pdsh options string (for getopt) -- initialized
 *    in _init_pdsh_options(), and appended to by modules that
 *    register new pdsh options.
 */    
static char *pdsh_options = NULL;

/*
 *  Pdsh personality
 */
static pers_t personality = DSH;

/*
 *  Return the current pdsh "personality"
 */
pers_t pdsh_personality(void)
{
    return personality;
}


static void
_init_pdsh_options()
{
    pdsh_options = Strdup(GEN_ARGS);
    if (personality == DSH) {
        xstrcat(&pdsh_options, DSH_ARGS);
    } else
        xstrcat(&pdsh_options, PCP_ARGS);
}

/*
 *  Check whether options in 'opt_table' are available for
 *    provisioning.  Register the option if the option's personality
 *    allows it.  'opt_table' points to the first module_option listed
 *    in the module.
 *
 *  Returns false if option is already used by pdsh or a pdsh module.
 *  Returns true if option was successfully registered.
 */
bool opt_register(struct pdsh_module_option *opt_table)
{
    struct pdsh_module_option *p;
  
    if (opt_table == NULL)
        return true;

    if (pdsh_options == NULL)
        _init_pdsh_options();
    
    /*  Don't register any options if we can't register all the options
     *   in this module 
     */
    for (p = opt_table; p && (p->opt != 0); p++) {
        if (  (personality & p->personality) 
           && (strchr(pdsh_options, p->opt) != NULL))
            return false;
    }

    for (p = opt_table; p && (p->opt != 0); p++) {
        /* register only if the personality allows this option */
        if (p->personality & personality) {
            xstrcatchar(&pdsh_options, p->opt);
            if (p->arginfo != NULL)
                xstrcatchar(&pdsh_options, ':');
        }
    }

    return true;
}

/*
 * Check current path dir, cwd, and argv0 for path to program
 */
char * _check_path(char *dir, char *cwd, char *argv0)
{
  char *abspath = NULL;

  if (*dir != '/') {
    abspath = Strdup(cwd);
    xstrcat(&abspath, "/");
  }
  xstrcat(&abspath, dir);
  xstrcat(&abspath, "/");
  xstrcat(&abspath, argv0);
  
  if (access(abspath, R_OK) == 0)
      return abspath;

  Free((void **) &abspath);

  return NULL;
}

/*
 *  Determine absolute path to the program name based on argv0
 */
char * _find_path(char *argv0) 
{
    char *abspath = NULL;
    char cwd[MAXPATHLEN];

    if (personality != PCP)
        goto done;

    if (*argv0 == '/') {
        /* is absolute path */
        abspath = Strdup(argv0);
        goto done;
    }

    if (getcwd(cwd, MAXPATHLEN) == NULL) 
        errx("%p: getcwd failed: %m\n"); 
      
    if (*argv0 == '.' || strchr(argv0, '/') != NULL) {
        /* path relative */
        abspath = Strdup(cwd);
        xstrcat(&abspath, "/");
        xstrcat(&abspath, argv0);
    }
    else {
      /* argv0 executed in PATH environment var */
      char *path, *dir, *p;
      
      if ((path = Strdup(getenv("PATH"))) == NULL)
        errx("%p: getenv PATH failed\n"); 
      
      dir = path;
      while ((p = strchr(dir,':')) != NULL) {
        *p = '\0';
        
        if (strlen(dir) > 0 && (abspath = _check_path(dir,cwd,argv0)) != NULL) {
            Free((void **) &path);
            goto done;
        }

        dir = ++p;
      }
      
      if (strlen(dir) > 0)
          abspath = _check_path(dir,cwd,argv0);
        
      Free((void **) &path);
    }

done:
    return abspath;
}

/*
 * Set defaults for various options.
 *	opt (IN/OUT)	option struct
 */
void opt_default(opt_t * opt, char *argv0)
{
    struct passwd *pw;

    opt->progname = xbasename(argv0);

    if (!strcmp(opt->progname, "pdsh") || !strcmp(opt->progname, "dsh"))
        personality = DSH;
    else if (!strcmp(opt->progname, "pdcp") 
            || !strcmp(opt->progname, "dcp")
            || !strcmp(opt->progname, "pcp") )
        personality = PCP;
    else
        errx("%p: program must be named pdsh/dsh/pdcp/dcp/pcp\n");

    if (pdsh_options == NULL)
        _init_pdsh_options();

    if ((pw = getpwuid(getuid())) != NULL) {
        strncpy(opt->luser, pw->pw_name, MAX_USERNAME);
        strncpy(opt->ruser, pw->pw_name, MAX_USERNAME);
        opt->luser[MAX_USERNAME - 1] = '\0';
        opt->ruser[MAX_USERNAME - 1] = '\0';
        opt->luid = pw->pw_uid;
    } else
        errx("%p: who are you?\n");

    opt->info_only = false;
    opt->test_range_expansion = false;
    opt->wcoll = NULL;
    opt->connect_timeout = CONNECT_TIMEOUT;
    opt->command_timeout = 0;
    opt->fanout = DFLT_FANOUT;
    opt->sigint_terminates = false;
    opt->infile_names = NULL;
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
    opt->ret_remote_rc = false;
    opt->cmd = NULL;
    opt->stdin_unavailable = false;
#if	HAVE_MAGIC_RSHELL_CLEANUP
    opt->separate_stderr = false;    /* save a socket per connection on aix */
#else
    opt->separate_stderr = true;
#endif

    /* PCP specific */
    opt->outfile_name = NULL;
    opt->recursive = false;
    opt->preserve = false;
    opt->pcp_server = false;
    opt->target_is_directory = false;
    opt->path_progname = _find_path(argv0);
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

    if ((rhs = getenv("PDSH_RCMD_TYPE")) != NULL)
        opt->rcmd_name = Strdup(rhs);

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
    int c;
    extern int optind;
    extern char *optarg;
    char *exclude_buf = NULL;

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
            opt->ret_remote_rc = true;
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
            opt->ruser[MAX_USERNAME - 1] = '\0';
            break;
        case 'r':              /* rcp: copy recursively */
            if (pdsh_personality() == PCP)
                opt->recursive = true;
            else
                goto test_module_option;
            break;
        case 'p':              /* rcp: preserve permissions */
            if (pdsh_personality() == PCP)
                opt->preserve = true;
            else
                goto test_module_option;
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
        case 'y':
            if (pdsh_personality() == PCP)
                opt->target_is_directory = true;  /* is target a dir? */
            else
                goto test_module_option;
            break;
        case 'z':
            if (pdsh_personality() == PCP)
                opt->pcp_server = true;          /* run PDCP server */
            else
                goto test_module_option;
            break;
        default: test_module_option:
            if (mod_process_opt(opt, c, optarg) < 0)
               _usage(opt);
        }
    }

    /*
     *  Load requested (or default) rcmd module 
     */
    if (mod_rcmd_load(opt) < 0)
        exit(1);

    /* DSH: build command */
    if (personality == DSH) {
        for (; optind < argc; optind++) {
            if (opt->cmd != NULL)
                xstrcat(&opt->cmd, " ");
            xstrcat(&opt->cmd, argv[optind]);
        }
    /* PCP: build file list */
    } else {
        if (!opt->infile_names)
            opt->infile_names = list_create(NULL);
        for (; optind < argc - 1; optind++)
            list_append(opt->infile_names, argv[optind]);
        if (optind < argc)
            xstrcat(&opt->outfile_name, argv[optind]);
    }

    /* ignore wcoll & -x option if we are running pcp server */
    if (!opt->pcp_server) {

        /*
         *  Attempt to grab wcoll from modules stack unless
         *    wcoll was set from -w.
         */
        if (mod_read_wcoll(opt) < 0)
            exit(1);

        /* handle -x option */
        if (exclude_buf != NULL && opt->wcoll) {
            /*
             * Delete any hosts in exclude_buf from the wcoll,  
             *  ignoring errors (except for an invalid hostlist
             *  in exclude_buf)
             */
            if (hostlist_delete(opt->wcoll, exclude_buf) <= 0) {
                if (errno == EINVAL)
                    errx ("%p: Invalid argument to -x \"%s\"\n", exclude_buf);
            }

            Free((void **) &exclude_buf);
        }
    }
}

/*
 * Trap a few option inconsitencies.
 *	opt (IN)	option struct
 */
bool opt_verify(opt_t * opt)
{
    bool verified = true;

    /*
     *  Call all post option processing functions for modules
     */
    if (mod_postop(opt) > 0)
        verified = false;

    /* can't prompt for command if stdin was used for wcoll */
    if (personality == DSH && opt->stdin_unavailable && !opt->cmd) {
        _usage(opt);
        verified = false;
    }

    if (!opt->pcp_server) { 
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
    }

    /* PCP: must have source and destination filename(s) */
    if (personality == PCP && !opt->pcp_server) {
        ListIterator i;
        char *name;

        if (!opt->outfile_name || list_is_empty(opt->infile_names)) {
            err("%p: pcp requires source and dest filenames\n");
            verified = false;
        }

        if (opt->target_is_directory) {
            err("%p: target is directory can only be specified with pcp server\n");
            verified = false;
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
    }

    /* PCP: verify options when -z option specified */
    if (personality == PCP  && opt->pcp_server) {
        if (opt->infile_names && !list_is_empty(opt->infile_names)) {
            err("%p: do not list source files with pcp server\n");
            verified = false;
        }

        if (!opt->outfile_name) {
            err("%p: output file must be specified with pcp server\n");
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

    if (personality == DSH) {
        out("-- DSH-specific options --\n");
        out("Separate stderr/stdout	%s\n",
            BOOLSTR(opt->separate_stderr));
        out("Path prepended to cmd	%s\n", STRORNULL(opt->dshpath));
        out("Appended to cmd         %s\n", STRORNULL(opt->getstat));
        out("Command:		%s\n", STRORNULL(opt->cmd));
    } else {
        out("-- PCP-specific options --\n");
        infile_str = _list_join(", ", opt->infile_names);
        if (infile_str) {
            out("Infile(s)		%s\n", infile_str);
            Free((void **) &infile_str);
        }
        out("Outfile			%s\n",
            STRORNULL(opt->outfile_name));
        out("Recursive		%s\n", BOOLSTR(opt->recursive));
        out("Preserve mod time/mode	%s\n", BOOLSTR(opt->preserve));
        out("Full program pathname	%s\n", STRORNULL(opt->path_progname));
        if (opt->pcp_server) {
            out("pcp server         	%s\n", BOOLSTR(opt->pcp_server));
            out("target is directory	%s\n", BOOLSTR(opt->target_is_directory));
        }
    }

    if (!opt->pcp_server) {

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
    if (opt->path_progname)
        Free((void **) &opt->path_progname);
    if (opt->infile_names)
        list_destroy(opt->infile_names);

    mod_rcmd_exit();
}

/*
 *  Returns a string of comma separated module names of type `type'
 *  Returns NULL if no modules of this type are loaded.
 */
static char * _module_list_string(char *type)
{
    List l      = NULL;
    char *names = NULL;

    if (mod_count(type) == 0)
        return NULL;

    l = mod_get_module_names(type);
    names = _list_join(",", l);
    list_destroy(l);

    return names;
}

static char *_rcmd_module_list(char *buf, int maxlen)
{
    int len, len2;
    char *rcmd_list = _module_list_string("rcmd");

    len = snprintf(buf, maxlen, "%s", rcmd_list ? rcmd_list : "(none)");
    if ((len < 0) || (len >= maxlen)) 
        goto done;

    if (mod_count("rcmd") > 1) {
        char *def = mod_rcmd_get_default_module();
        len2 = snprintf ( buf+len, maxlen-len, " (default: %s)",
                          def ? def : "none" );
        if (len2 < 0)
            len = -1;
        else
            len += len2;
    }

   done:
    if ((len < 0) || (len > maxlen)) 
        snprintf(buf + maxlen - 12, 12, "[truncated]"); 

    Free ((void **) &rcmd_list);
    buf[maxlen - 1] = '\0';
    return buf;
}

/*
 * Spit out all the options and their one-line synopsis for the user, 
 * then exit.
 */
static void _usage(opt_t * opt)
{
    char buf[1024];

    if (personality == DSH) {
        err(OPT_USAGE_DSH);
#if	HAVE_MAGIC_RSHELL_CLEANUP
        err(OPT_USAGE_STDERR);
#endif
    } else                      /* PCP */
        err(OPT_USAGE_PCP);

    err(OPT_USAGE_COMMON);

    mod_print_all_options(18);

    err("available rcmd modules: %s\n", _rcmd_module_list(buf, 1024));

    exit(1);
}


static void _show_version(void)
{
    char buf[1024];
    extern char *pdsh_version;
    char *misc_list = _module_list_string("misc");

    printf("%s\n", pdsh_version);
    printf("rcmd modules: %s\n", _rcmd_module_list(buf, 1024));
    printf("misc modules: %s\n", misc_list ? misc_list : "(none)");

    Free((void **) &misc_list);

    exit(0);
}


/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

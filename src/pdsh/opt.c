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

#include <regex.h>
#include <ctype.h>

#include "src/common/hostlist.h"
#include "src/common/err.h"
#include "src/common/list.h"
#include "src/common/split.h"
#include "src/common/xstring.h"
#include "src/common/xmalloc.h"
#include "dsh.h"                
#include "opt.h"
#include "wcoll.h"
#include "mod.h"
#include "rcmd.h"

/*
 *  Fallback maximum username length if sysconf(_SC_LOGIN_NAME_MAX) not
 *   available or fails. (buffers will be this size + 1 for NUL termination)
 */
#define DEFAULT_MAX_USERNAME_LENGTH 16

#define OPT_USAGE_DSH "\
Usage: pdsh [-options] command ...\n\
-S                return largest of remote command return values\n\
-k                fail fast on connect failure or non-zero return code\n"

/* -s option only useful on AIX */
#if	HAVE_MAGIC_RSHELL_CLEANUP
#define OPT_USAGE_STDERR "\
-s                separate stderr and stdout\n"
#endif


#define OPT_USAGE_PCP "\
Usage: pdcp [-options] src [src2...] dest\n\
-r                recursively copy files\n\
-p                preserve modification time and modes\n\
-e PATH           specify the path to pdcp on the remote machine\n"
/* undocumented "-y"  target must be directory option */
/* undocumented "-z"  run pdcp server option */
/* undocumented "-Z"  run pdcp client option */

#define OPT_USAGE_RPCP "\
Usage: rpdcp [-options] src [src2...] dir\n\
-r                recursively copy files\n\
-p                preserve modification time and modes\n"
/* undocumented "-y"  target must be directory option */
/* undocumented "-z"  run pdcp server option */
/* undocumented "-Z"  run pdcp client option */

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
-M name,...       select one or more misc modules to initialize first\n\
-N                disable hostname: labels on output lines\n\
-L                list info on all loaded modules and exit\n"
/* undocumented "-T testcase" option */
/* undocumented "-Q" option */
/* undocumented "-K" option -  keep domain name in output */

#if	HAVE_MAGIC_RSHELL_CLEANUP
#define DSH_ARGS	"sSk"
#else
#define DSH_ARGS    "Sk"
#endif
#define PCP_ARGS	"pryzZe:"
#define GEN_ARGS	"hLNKR:M:t:cqf:w:x:l:u:bI:dVT:Q"


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

/*
 *  Remote argv array
 */
static char **remote_argv;
static int    remote_argc;

const char **pdsh_remote_argv (void)
{
    return (const char **) remote_argv;
}

int pdsh_remote_argc (void)
{
    return remote_argc;
}

/*
 *  List of explicitly excluded hosts and regex filter options:
 */
static List exclude_list = NULL;
static List regex_list = NULL;

static void _usage(opt_t * opt);
static void _show_version(void);
static int wcoll_args_process (opt_t *opt, char *args);
static void wcoll_apply_regex (opt_t *opt, List regexs);
static void wcoll_apply_excluded (opt_t *opt, List excludes);
static void wcoll_expand (opt_t *opt);

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
 *   Return a value for the max login name length (username buffer length)
 */
static int login_name_max_len (void)
{
    static int maxnamelen = -1;

    if (maxnamelen < 0) {
#ifdef _SC_LOGIN_NAME_MAX
        errno = 0;
        if ((maxnamelen = sysconf (_SC_LOGIN_NAME_MAX)) <= 0) {
            err ("%p: sysconf(LOGIN_NAME_MAX): %m\n");
            maxnamelen = DEFAULT_MAX_USERNAME_LENGTH;
        }
#else
        maxnamelen = DEFAULT_MAX_USERNAME_LENGTH;
#endif
    }

    return (maxnamelen);
}

static void copy_username (char *dst, const char *src)
{
    int maxlen = login_name_max_len ();

    if (strlen (src) > maxlen)
        errx ("%p: Fatal: username '%s' exceeds max username length (%d)\n",
                src, maxlen);

    strcpy (dst, src);
}

/*
 * Set defaults for various options.
 *	opt (IN/OUT)	option struct
 */
void opt_default(opt_t * opt, char *argv0)
{
    struct passwd *pw;

    opt->progname = xbasename(argv0);
    opt->luser = Malloc (login_name_max_len () + 1);
    opt->ruser = Malloc (login_name_max_len () + 1);

    opt->reverse_copy = false;

    if (!strcmp(opt->progname, "pdsh") || !strcmp(opt->progname, "dsh"))
        personality = DSH;
    else if (!strcmp(opt->progname, "pdcp") 
            || !strcmp(opt->progname, "dcp")
            || !strcmp(opt->progname, "pcp") )
        personality = PCP;
    else if (!strcmp(opt->progname, "rpdcp")) {
        personality = PCP;
        opt->reverse_copy = true;
    } else
        errx("%p: program must be named pdsh/dsh/pdcp/dcp/pcp/rpdcp\n");

    if (pdsh_options == NULL)
        _init_pdsh_options();

    if ((pw = getpwuid(getuid())) != NULL) {
        copy_username (opt->luser, pw->pw_name);
        copy_username (opt->ruser, pw->pw_name);
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
    opt->misc_modules = NULL;

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

    opt->local_program_path = _find_path(argv0);

    /*
     *  By default assume remote path to pdsh/pdcp is the same
     *   as local path: (overridden with pdpc -e or PDSH_REMOTE_PDCP_PATH).
     */
    opt->remote_program_path = Strdup(opt->local_program_path);

    /* PCP specific */
    opt->outfile_name = NULL;
    opt->recursive = false;
    opt->preserve = false;
    opt->pcp_server = false;
    opt->target_is_directory = false;
    opt->pcp_client = false;
    opt->pcp_client_host = NULL;

    return;
}

static int string_to_int (const char *val, int *p2int)
{
    char *p;
    long n;

    errno = 0;
    n = strtoul (val, &p, 10);
    if (errno || (*p != '\0'))
        return (-1);

    *p2int = (int) n;

    return (0);
}

/*
 * Override default options with environment variables.
 *	opt (IN/OUT)	option struct	
 */
void opt_env(opt_t * opt)
{
    char *rhs;

    if ((rhs = getenv("FANOUT")) != NULL)
        if (string_to_int (rhs, &opt->fanout) < 0)
            errx ("%p: Invalid environment variable FANOUT=%s\n", rhs);

    if ((rhs = getenv("PDSH_CONNECT_TIMEOUT")) != NULL)
        if (string_to_int (rhs, &opt->connect_timeout) < 0)
            errx ("%p: Invalid environment variable PDSH_CONNECT_TIMEOUT=%s\n", rhs);

    if ((rhs = getenv("PDSH_COMMAND_TIMEOUT")) != NULL)
        if (string_to_int (rhs, &opt->command_timeout) < 0)
            errx ("%p: Invalid environment variable PDSH_COMMAND_TIMEOUT=%s\n", rhs);

    if ((rhs = getenv("PDSH_RCMD_TYPE")) != NULL)
        opt->rcmd_name = Strdup(rhs);

    if ((rhs = getenv("PDSH_MISC_MODULES")) != NULL)
        opt->misc_modules = Strdup(rhs);

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

    if (pdsh_personality() == PCP) {
        if ((rhs = getenv ("PDSH_REMOTE_PDCP_PATH")) != NULL) {
            Free ((void **) &opt->remote_program_path);
            opt->remote_program_path = Strdup (rhs);
        }
    }
}


/*
 *  Process any options that need to be handled early, i.e.
 *   before modules are loaded.
 */
void opt_args_early (opt_t * opt, int argc, char *argv[])
{
    int c;
    extern int optind;
    extern char *optarg;
    extern int opterr;
#ifdef __linux
    int pc = 0;
#endif

    /*
     *  Disable error reporting from getopt during early processing,
     *   since we won't have access to all the options provided by
     *   dlopened modules.
     */
    opterr = 0;

#ifdef __linux
    if (!getenv("POSIXLY_CORRECT")) {
        /* Tell glibc getopt to stop eating after the first non-option arg */
        putenv("POSIXLY_CORRECT=1");
        pc = 1;
    }
#endif
    while ((c = getopt(argc, argv, pdsh_options)) != EOF) {
        switch (c) {
            case 'M':
                if (opt->misc_modules)
                    Free ((void **) &opt->misc_modules);
                opt->misc_modules = Strdup (optarg);
                break;
        }
    }
#ifdef __linux
    if (pc) {
        unsetenv("POSIXLY_CORRECT");
    }
#endif
}

static void wcoll_append_excluded (opt_t *opt, char *exclude_args)
{
    List l = list_split (",", exclude_args);
    ListIterator i = list_iterator_create (l);
    char *s;

    while ((s = list_next (i))) {
        char *p = NULL;
        xstrcatchar (&p, '-');
        xstrcat (&p, s);
        wcoll_args_process (opt, p);
        Free ((void **) &p);
    }

    list_iterator_destroy (i);
    list_destroy (l);
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
    extern int opterr;

    /*
     *  Reset optind after opt_args_early.
     */
    optind = 1;

    /*
     * Reinstate getopt error reporting
     */
    opterr = 1;

    while ((c = getopt(argc, argv, pdsh_options)) != EOF) {
        switch (c) {

        /*
         *  The following options were handled in opt_args_early() :
         */
        case 'M':
            break;

        /*  Continue processing regular options...
         */
        case 'N':
            opt->labels = false;
            break;
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
            if (string_to_int (optarg, &opt->fanout) < 0)
                errx ("%p: Invalid fanout `%s' passed to -f.\n", optarg);
            break;
        case 'w':              /* target node list */
            if (strcmp(optarg, "-") == 0)
                wcoll_args_process (opt, "^-");
            else
                wcoll_args_process (opt, optarg);
            break;
        case 'x':              /* exclude node list */
            wcoll_append_excluded (opt, optarg);
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
            copy_username (opt->ruser, optarg);
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
        case 'e':
            if (pdsh_personality() == PCP) {
                Free ((void **) &opt->remote_program_path);
                opt->remote_program_path = Strdup(optarg);
            }
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
        case 'K':              /* don't strip host domain in output */
            err_no_strip_domain (); 
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
        case 'Z':
            if (pdsh_personality() == PCP)
                opt->pcp_client = true;          /* run PDCP client */
            else
                goto test_module_option;
            break;
        case 'k':
            opt->kill_on_fail = true;
            break;
        default: test_module_option:
            if (mod_process_opt(opt, c, optarg) < 0)
               _usage(opt);
        }
    }

    /*
     *  Load default module for all hosts (unless overridden)
     */
    if (opt->rcmd_name == NULL)
        opt->rcmd_name = Strdup(rcmd_get_default_module ());
    if (opt->rcmd_name != NULL)
        if (rcmd_register_default_rcmd(opt->rcmd_name) < 0)
            exit(1);


    /* 
     *  Save beginning of remote argv in case something needs
     *   to view the unadulterated version (after shell quoting
     *   applied, etc.)
     */
    remote_argc = argc - optind;
    remote_argv = argv + optind;

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
        if (optind < argc) {
            /* If this is the initial pdcp call, the last argument is
             * the output file/dir.  If this is the pcp_client, the
             * last argument is the hostname used for connecting.
             */
            if (opt->pcp_client)
                xstrcat(&opt->pcp_client_host, argv[optind]);
            else
                xstrcat(&opt->outfile_name, argv[optind]);
        }
    }

    /* ignore wcoll filtering when running pcp server */
    if (opt->pcp_server)
        return;

    /*
     *  Give modules a chance to fill in wcoll if it hasn't been already:
     */
    if (mod_read_wcoll (opt) < 0)
        exit (1);

    /*
     *  If wcoll is still empty, try WCOLL env variable:
     */
    if (opt->wcoll == NULL) {
        char *val = getenv ("WCOLL");
        if (val != NULL)
            opt->wcoll = read_wcoll (val, NULL);
    }

    if (opt->wcoll) {
        /*
         *  Now apply wcoll filtering
         */
        if (exclude_list) {
            wcoll_apply_excluded (opt, exclude_list);
            list_destroy (exclude_list);
        }
        if (regex_list) {
            wcoll_apply_regex (opt, regex_list);
            list_destroy (regex_list);
        }

        /*
         *  Finally, re-expand wcoll to allow two sets of brackets.
         *   (For historical compatibility)
         */
        wcoll_expand (opt);
    }
}

static void wcoll_expand (opt_t *opt)
{
    hostlist_t hl = opt->wcoll;
    char *hosts;

    /*
     *  Create new hostlist for wcoll
     */
    opt->wcoll = hostlist_create ("");
    while ((hosts = hostlist_shift (hl))) {
        hostlist_push (opt->wcoll, hosts);
        free (hosts);
    }

    hostlist_destroy (hl);
}


/* 
 * Check if infile_names are legit.
 */
static int
_infile_names_check(opt_t * opt)
{
    bool verified = true;
    ListIterator i;
    char *name;

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

    if (!opt->pcp_server && !opt->pcp_client) { 
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
    if (personality == PCP && !opt->pcp_server && !opt->pcp_client) {
        if (!opt->outfile_name || list_is_empty(opt->infile_names)) {
            err("%p: pcp requires source and dest filenames\n");
            verified = false;
        }

        if (opt->target_is_directory) {
            err("%p: target is directory can only be specified with pcp server\n");
            verified = false;
        }

        /* If reverse copy, the infiles need not exist locally */
        if (!opt->reverse_copy) {
            if (!_infile_names_check(opt))
                verified = false;
        }

        /* If reverse copy, the destination must be a directory */
        if (opt->reverse_copy && opt->outfile_name) {
            struct stat statbuf;

            if (stat(opt->outfile_name, &statbuf) < 0) {
                err("%p: can't stat %s\n", opt->outfile_name);
                verified = false;
            }

            if (!S_ISDIR(statbuf.st_mode)) {
                err("%p: reverse copy dest must be a directory\n");
                verified = false;
            }
        }
    }

    /* PCP: server and client sanity check */
    if (personality == PCP && opt->pcp_server && opt->pcp_client) {
        err("%p: pcp server and pcp client cannot both be set\n");
        verified = false;
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

        if (opt->pcp_client_host) {
            err("%p: pcp client host should not be specified with pcp server\n");
            verified = false;
        }

        if (opt->reverse_copy) {
            err("%p: reverse copy cannot be specified with pcp server\n");
            verified = false;
        }
    }

    /* PCP: verify options when -Z option specified */
    if (personality == PCP  && opt->pcp_client) {

        opt->reverse_copy = false;

        if (!opt->infile_names || list_is_empty(opt->infile_names)) {
            err("%p: list source files required for pcp client\n");
            verified = false;
        }

        if (opt->outfile_name) {
            err("%p: output file should not be specified with pcp client\n");
            verified = false;
        }

        if (!opt->pcp_client_host) {
            err("%p: pcp client host must be specified with pcp client\n");
            verified = false;
        }

        /* If reverse copy the infiles should exist locally */
        if (opt->infile_names && !_infile_names_check(opt))
            verified = false;
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
void opt_list(opt_t * opt)
{
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
        char infiles [4096];
        out("-- PCP-specific options --\n");
        if (list_join (infiles, sizeof (infiles), ", ", opt->infile_names))
            out("Infile(s)		%s\n", infiles);
        out("Outfile			%s\n", STRORNULL(opt->outfile_name));
        out("Recursive		%s\n", BOOLSTR(opt->recursive));
        out("Preserve mod time/mode	%s\n", BOOLSTR(opt->preserve));
        if (opt->pcp_server) {
            out("pcp server         	%s\n", BOOLSTR(opt->pcp_server));
            out("target is directory	%s\n", BOOLSTR(opt->target_is_directory));
        }
    }

    if (!opt->pcp_server) {
        out("Full program pathname	%s\n", STRORNULL(opt->local_program_path));
        out("Remote program path	%s\n",
                STRORNULL(opt->remote_program_path));
        out("\n-- Generic options --\n");
        out("Local username		%s\n", opt->luser);
        out("Local uid     		%d\n", opt->luid);
        out("Remote username		%s\n", opt->ruser);
        out("Rcmd type		%s\n", STRORNULL(opt->rcmd_name));
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
    if (opt->misc_modules != NULL)
        Free((void **) &opt->misc_modules);
    if (pdsh_options)
        Free((void **) &pdsh_options);
    if (opt->dshpath)
        Free((void **) &opt->dshpath);
    if (opt->local_program_path)
        Free((void **) &opt->local_program_path);
    if (opt->remote_program_path)
        Free((void **) &opt->remote_program_path);
    if (opt->infile_names)
        list_destroy(opt->infile_names);
    if (opt->luser)
        Free((void **) &opt->luser);
    if (opt->ruser)
        Free((void **) &opt->ruser);

    rcmd_exit();
}

/*
 *  Returns a string of comma separated module names of type `type'
 *  Returns NULL if no modules of this type are loaded.
 */
static int _module_list_string(char *type, char *buf, int len)
{
    List l = NULL;
    int  n = 0;

    if (mod_count(type) == 0)
        return (0);

    l = mod_get_module_names(type);
    n = list_join(buf, len, ",", l);
    list_destroy(l);

    return (n);
}

static int _module_list_uninitialized (char *type, char *buf, int len)
{
    List l = NULL;
    int  n = 0;

    if (mod_count(type) == 0)
        return (0);

    l = mod_get_uninitialized_module_names(type);
    n = list_join(buf, len, ",", l);
    list_destroy(l);

    return (n);

}

static char *_rcmd_module_list(char *buf, int maxlen)
{
    int len, len2;
    char rbuf [1024];
    int n;

    n = _module_list_string ("rcmd", rbuf, sizeof (rbuf));

    len = snprintf(buf, maxlen, "%s", n ? rbuf : "(none)");
    if ((len < 0) || (len >= maxlen)) 
        goto done;

    if (mod_count("rcmd") > 1) {
        char *def = rcmd_get_default_module();
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
    } else if (!opt->reverse_copy) /* PCP */
        err(OPT_USAGE_PCP);
    else 
        err(OPT_USAGE_RPCP);

    err(OPT_USAGE_COMMON);

    mod_print_all_options(18);

    err("available rcmd modules: %s\n", _rcmd_module_list(buf, 1024));

    exit(1);
}


static void _show_version(void)
{
    char buf[1024];
    extern char *pdsh_version;
    int n;

    printf("%s\n", pdsh_version);
    printf("rcmd modules: %s\n", _rcmd_module_list(buf, sizeof (buf)));

    n = _module_list_string("misc", buf, sizeof (buf));
    printf("misc modules: %s", n ? buf : "(none)");

    if ((n = _module_list_uninitialized ("misc", buf, sizeof (buf)))) {
        printf (" (*conflicting: %s)\n", buf);
	printf ("[* To force-load a conflicting module,"
	        " use the -M <name> option]\n");
    }
    else
        printf ("\n");

    exit(0);
}

/*
 *  Take a string `hosts' possibly of form "rcmd_type:user@hostlist" and
 *    place the hostlist part of the string in *hptr and the rcmd part
 *    of the string in rptr. Returns nonzero if an rcmd_type was found,
 *    zero otherwise (in which case rptr is not touched).
 */
static int get_host_rcmd_type (char *hosts, char **rptr, char **hptr, 
                               char **uptr)
{
    char *p = hosts;
    char *q;
    *hptr = hosts;

    p = strchr (hosts, ':');
    q = strchr (hosts, '@');

    if (p && q && p > q)
        errx ("Host spec \"%s\" not of form [rcmd_type:][user@]hosts\n", hosts);

    /*
     *  If we found a single ':' character, then everything
     *   preceeding that is the rcmd type.  Otherwise, we ignore
     *   presence of all colons. This can be done b/c even if there
     *   were another colon later in the string, the string preceeding
     *   it can not be an rcmd type since colon is not extant in rcmd
     *   type names.
     */
    if (p && (*(p+1) != ':')) {
        *rptr = *hptr;
        *p++ = '\0';
        *hptr = p;
    }

    /*
     *  If we found a '@' char then what precedes it is username.
     */
    if (q) {
        *uptr = *hptr;
        *q++ = '\0';
        *hptr = q;
    }

    return (1);
}

void free_f (void *x)
{
    Free (&x);
}

struct regex_info {
    int     exclude;
    int     cflags;
    int     eflags;
    int     compiled;
    char *  pattern;
    regex_t reg;
};

void regex_info_destroy (struct regex_info *re)
{
    if (re->compiled)
        regfree (&re->reg);
    if (re->pattern)
        Free ((void **) &re->pattern);
    Free ((void **) &re);
}

struct regex_info * regex_info_create (const char *r, int exclude)
{
    int rc;
    struct regex_info *re = Malloc (sizeof (*re));

    re->pattern = Strdup (r);
    re->exclude = exclude;
    re->cflags =  REG_EXTENDED | REG_NOSUB;
    re->eflags =  0;
    re->compiled = 0;

    if ((rc = regcomp (&re->reg, re->pattern, re->cflags)) != 0) {
        char msg [4096];
        regerror (rc, &re->reg, msg, sizeof (msg));
        err ("%p: Error %s pattern \"%s\": %s\n",
                re->exclude ? "excluding" : "matching",
                re->pattern, msg);
        regex_info_destroy (re);
        return (NULL);
    }

    re->compiled = 1;
    return (re);
}


void hostlist_filter_regex (hostlist_t hl, struct regex_info *re)
{
    char *host;
    hostlist_iterator_t i;

    i = hostlist_iterator_create (hl);
    while ((host = hostlist_next (i))) {
        int rc = regexec (&re->reg, host, 0, NULL, re->eflags);
        if ((re->exclude && rc == 0) || (!re->exclude && rc == REG_NOMATCH))
            hostlist_remove (i);
        free (host);
    }
    hostlist_iterator_destroy (i);
}


static void list_push_hostlist (List l, hostlist_t hl)
{
    size_t n = 4096;
    char *s = Malloc (n);

    while ((hostlist_ranged_string (hl, n-1, s) < 0) && (n*=2 < 0x7fffff)) {
        Realloc ((void **) &s, n);
    }

    list_push (l, s);
}


static void hostlist_assign (hostlist_t *hlp, hostlist_t hl2)
{
    if (*hlp == NULL)
        *hlp = hostlist_create ("");
    hostlist_push_list (*hlp, hl2);
}

static int wcoll_arg_process (char *arg, opt_t *opt)
{
    struct regex_info *re;
    int excluded = 0;
    char *p = arg;

    if (exclude_list == NULL)
        exclude_list = list_create (free_f);

    if (regex_list == NULL)
        regex_list = list_create ((ListDelF) regex_info_destroy);

    /*
     *  Check for excluded arg
     */
    if (*p == '-') {
        excluded = 1;
        p++;
    }

    /*
     *  Move past any leading whitespace
     */
    while (isspace (*p))
        p++;

    if (*p == '^') {
        hostlist_t hl = read_wcoll (p+1, NULL);
        if (hl == NULL)
            errx ("%p: Error reading wcoll: %s: %m\n", p+1);
        if (excluded)
            list_push_hostlist (exclude_list, hl);
        else
            hostlist_assign (&opt->wcoll, hl);
        hostlist_destroy (hl);
    }
    else if (*p == '/') {
        int len;

        ++p;
        len = strlen (p);
        if (p [len - 1] == '/')
            p [len - 1] = '\0';

        if ((re = regex_info_create (p, excluded)) == NULL)
            errx ("%p: Fatal error\n");

        list_push (regex_list, re);
    }
    else {
        if (excluded) {
            list_push (exclude_list, Strdup (p));
        }
        else {
            char *rcmd_type = NULL;
            char *hosts, *user = NULL;

            if (!opt->wcoll)
                opt->wcoll = hostlist_create ("");

            get_host_rcmd_type (p, &rcmd_type, &hosts, &user);
            hostlist_push (opt->wcoll, hosts);
            if (rcmd_type || user) {
                if (rcmd_register_defaults (hosts, rcmd_type, user) < 0)
                    errx ("%p: Failed to register rcmd \"%s\" for \"%s\"\n",
                            rcmd_type, hosts);
            }
        }
    }

    return 0;
}

static int wcoll_args_process (opt_t *opt, char * args)
{
    int rc;
    List l = list_split (",", args);
    rc = list_for_each (l, (ListForF) wcoll_arg_process, opt);
    list_destroy (l);
    return (rc);
}

static void wcoll_apply_regex (opt_t *opt, List regexs)
{
    struct regex_info *re;
    ListIterator i;

    if (!opt->wcoll || !regexs)
        return;

    /*
     *  filter any supplied regular expression args
     */
    i = list_iterator_create (regexs);
    while ((re = list_next (i)))
        hostlist_filter_regex (opt->wcoll, re);
    list_iterator_destroy (i);
}

static void wcoll_apply_excluded (opt_t *opt, List excludes)
{
    ListIterator i;
    char *arg;

    if (!opt->wcoll || !excludes)
        return;

    /*
     *  filter explicitly excluded hosts:
     */
    i = list_iterator_create (excludes);
    while ((arg = list_next (i)))
        hostlist_delete (opt->wcoll, arg);
    list_iterator_destroy (i);
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

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

#ifndef _OPT_INCLUDED
#define _OPT_INCLUDED

#if	HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>          /* for uid_t */

#include "list.h"
#include "hostlist.h"

#ifndef _BOOL_DEFINED
#define _BOOL_DEFINED
typedef enum { false, true } bool;
#endif

#define MAX_USERNAME	9
#define MAX_GENDATTR	64

#define RC_MAGIC	"XXRETCODE:"

#define RC_FAILED	254     /* -S exit value if any hosts fail to connect */

/* set to 0x1 and 0x2 to allow bit masking */
typedef enum { DSH = 0x1, PCP = 0x2} pers_t;

typedef struct {

    /* common options */
    char *progname;             /* argv[0] */
    pers_t personality;
    bool debug;                 /* -d */
    bool info_only;             /* -q */
    bool test_range_expansion;  /* -Q (implies -q) */
    bool sdr_verify;            /* -v */
    bool sdr_global;            /* -G */
    bool altnames;              /* -i */
    bool sigint_terminates;     /* -b */
    hostlist_t wcoll;           /* target node list (-w, WCOLL, or stdin) */
    char luser[MAX_USERNAME];   /* local username */
    uid_t luid;                 /* uid for above */
    char ruser[MAX_USERNAME];   /* remote username (-l or default) */
    int fanout;                 /* (-f, FANOUT, or default) */
    int connect_timeout;
    int command_timeout;
    int nprocs;                 /* -n nprocs */

    char *rcmd_name;            /* -R name   */
    bool resolve_hosts;         /* Set optionally by rcmd modules */

    bool kill_on_fail;          

    /* DSH-specific options */
    bool separate_stderr;       /* -s */
    bool stdin_unavailable;     /* set if stdin used for WCOLL */
    char *cmd;
    char *dshpath;              /* optional PATH command prepended to cmd */
    char *getstat;              /* optional echo $? appended to cmd */
    bool labels;                /* display host: before output */

    /* PCP-specific options */
    bool preserve;              /* -p */
    bool recursive;             /* -r */
    List infile_names;        /* -I or pcp source spec */
    char *outfile_name;         /* pcp dest spec */

} opt_t;

void opt_default(opt_t *);
void opt_env(opt_t *);
void opt_args(opt_t *, int, char **);
bool opt_verify(opt_t *);
void opt_list(opt_t *);
void opt_free(opt_t *);


/*
 * Structure for pdsh modules to export new options. 
 * 
 * Module should define a table of options as:
 *
 *     struct pdsh_module_option pdsh_module_opts[] = { ... };
 *
 *   which will be read by the module loader. The module loader
 *   (see mod.c) will call opt_register for each of the defined
 *   options. If any option fails to register, the module will
 *   be unloaded and a warning message printed.
 */ 

typedef int (*optFunc)(opt_t *opt, int optopt, char *optarg);

struct pdsh_module_option {
	char    opt;        /* option character                          */
        char   *arginfo;    /* one word descr of arg if option takes one 
                               If NULL, option takes no arg              */
	char   *descr;      /* short description of option               */
	optFunc f;          /* callback function for option processing   */
};

#define PDSH_OPT_TABLE_END { 0, NULL, NULL, NULL }


bool opt_register(struct pdsh_module_option *popt);

#endif                          /* OPT_INCLUDED */

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

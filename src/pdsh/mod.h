/*****************************************************************************\
 *  $Id$
 *****************************************************************************
 *  Copyright (C) 2001-2002 The Regents of the University of California.
 *  Produced at Lawrence Livermore National Laboratory (cf, DISCLAIMER).
 *  Written by Mark Grondona <mgrondona@llnl.gov>.
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
#ifndef _MOD_H
#define _MOD_H

#if HAVE_CONFIG_H
#  include <config.h>
#endif 

#include "src/pdsh/opt.h"

typedef struct module_components * mod_t;

/*
 *  Initialize the module loader interface
 *    Returns 0 for Success and -1 for Failure.
 */
int mod_init(void);

/*
 *  Finalize and close the module loader interface.
 *    Cycles through list of loaded modules and runs each modules
 *    "exit" routine if one was exported. Then frees memory associated
 *    with the module and unloads it. 
 *
 *  Returns 0 for Success and -1 for Failure.
 */
int mod_exit(void);

/*
 *  Load all modules from specified directory. 
 *    Directory must be owned by the current user and not writable 
 *    by any other user.  After successfully loading each module, 
 *    the module's "init" routine is called and module command line 
 *    options are registered.  The module is not loaded if init 
 *    returns < 0 or any module option command be registered.
 *
 *  If modules are being compiled statically, The directory argument
 *    is ignored.
 *
 *  Returns 0 for Success and -1 for Failure.
 */
int mod_load_modules(const char *dir);

/*
 *  List information about all loaded modules to stdout.
 */
void mod_list_module_info(void);

/*
 *  Traverse through loaded modules and attempt to process 
 *    option `opt' with argument `arg.'
 *
 *  Note: Only one module exporting a given option can be loaded
 *    at a time. This is enforced on a first-come-first-served basis.
 */
int mod_process_opt(opt_t *pdsh_opts, int opt, char *arg);

/*
 *  Traverses list of loaded modules, calling any exported "read_wcoll" 
 *    routines. Appends any returned results onto opt->wcoll.
 *
 *  This routine should only be called from within pdsh/opt.c after
 *    option processing is complete, but before mod_postop().
 *
 *  Returns -1 for failure, 0 for success.
 *
 */
int mod_read_wcoll(opt_t *pdsh_opts);

/*
 *  Traverse list of loaded modules and call any exported "postop" routines.
 *
 *  Returns the total number of errors.
 *
 */
int mod_postop(opt_t *pdsh_opts);

/*
 *  Search list of loaded modules for a module with given type and name.
 *    Returns the module if found, NULL if no match.
 */
mod_t mod_get_module(const char *type, const char *name);

/*
 *  Return the number of loaded modules of type `"type." Returns the
 *    total number of modules if type is NULL.
 */
int mod_count(char *type);

/*
 *  Build list of module names of type "type"
 *  List contains all module names if type is NULL.
 */
List mod_get_module_names(char *type);

/*
 * Print all options provided by modules
 *   Justify option description starting on given column.
 */
void mod_print_all_options(int column);

/* 
 *  Print options for module "mod"
 */
void mod_print_options(mod_t mod, int descr_column);

/*
 * Functions that may be exported by any pdsh module
 *   via a pdsh_module_operations structure. 
 */
typedef int        (*ModInitF)      (void);
typedef int        (*ModExitF)      (void);
typedef hostlist_t (*ModReadWcollF) (opt_t *);
typedef int        (*ModPostOpF)    (opt_t *);

/*
 * Functions that may be exported by any rcmd module
 *   via a pdsh_rcmd_operations structure. 
 */
typedef int        (*RcmdInitF)     (opt_t *);
typedef int        (*RcmdSigF)      (int, int);
typedef int        (*RcmdF)         (char *, char *, char *, char *, char *,
                                     int, int *);
/*
 *  Module accessor functions. Return module name, type, and
 *    look up additional exported symbols in given module.
 *
 *  All return a pointer to the desired value if successful, or NULL
 *    on failure.
 */
char *    mod_get_name(mod_t mod);
char *    mod_get_type(mod_t mod);
RcmdInitF mod_get_rcmd_init(mod_t mod);
RcmdSigF  mod_get_rcmd_signal(mod_t mod);
RcmdF     mod_get_rcmd(mod_t mod);


/* 
 * Store all module operations of a module 
 */
struct pdsh_module_operations {
    ModInitF      init;         /* Called just after module is loaded      */
    ModExitF      exit;         /* Called just before module unloaded      */

    ModReadWcollF read_wcoll;   /* Called if wcoll is not initialized at
                                   end of option processing. First wcoll
                                   returned by a module will be used.      */ 

    ModPostOpF    postop;       /* Called after argv option processing     */
};

/* 
 * Stores all rcmd operations of a module 
 */
struct pdsh_rcmd_operations {
    RcmdInitF  rcmd_init;
    RcmdSigF   rcmd_signal;
    RcmdF      rcmd;
};

/* 
 * Stores all information about a module 
 */
struct pdsh_module {
    char *type;        /* module type, i.e. Jedi */
    char *name;        /* module name, i.e. Yoda */ 
    char *author;      /* module author, i.e. George Lucas */
    char *descr;       /* module description, i.e. "Run pdsh with the force */
    int personality;   /* personality mask for module (DSH, PCP, or DSH|PCP */

    struct pdsh_module_operations *mod_ops;
    struct pdsh_rcmd_operations   *rcmd_ops;
    struct pdsh_module_option     *opt_table;
};

#endif /* !_MOD_H */

/* 
 * vi: tabstop=4 shiftwidth=4 expandtab
 */

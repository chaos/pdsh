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
#if HAVE_CONFIG_H
#  include <config.h>
#endif
#if HAVE_FEATURES_H
#  include <features.h>
#endif

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <assert.h>
#include <string.h>

#if STATIC_MODULES
#include "static_modules.h"
#else
#include <dlfcn.h>
#endif 

#include "src/common/err.h"
#include "src/common/xmalloc.h"
#include "src/common/xstring.h"
#include "src/common/hostlist.h"
#include "src/common/list.h"
#include "src/common/split.h"
#include "mod.h"

/*
 * pdsh/322: Workaround apparent bug in glibc 2.2.4 which 
 * occaisionally causes LinuxThreads manager thread to
 * segfault at exit. (Disable dlclose() and lt_dlexit()
 * in these versions of glibc)
 */
#if (__GLIBC__ == 2) && (__GLIBC_MINOR__ < 3)
#  define PREVENT_DLCLOSE_BUG  1
#endif

/*
 *  Components of a module.
 */
struct module_components {
#ifndef NDEBUG
#define MOD_MAGIC 0xc0c0b0b
    int magic;
#endif

#if !STATIC_MODULES
    void *handle;
#endif
    char *filename;

    int priority;
    int initialized;

    struct pdsh_module *pmod;
};


typedef enum permission_error {
    DIR_OK,
    DIR_NOT_DIRECTORY,
    DIR_BAD_OWNER,
    DIR_WORLD_WRITABLE
} perm_error_t;


/* 
 *  Static function prototypes:
 */
#if STATIC_MODULES
static int  _mod_load_static_modules(void);
static int  _mod_load_static(int);
#else
static int  _mod_load_dynamic_modules(const char *, opt_t *);
static int  _mod_load_dynamic(const char *);
static int  _cmp_filenames(mod_t, char *);
static int  _is_loaded(char *filename); 
static bool _path_permissions_ok(const char *dir, uid_t pdsh_owner);
static perm_error_t  _dir_permission_error(struct stat *, uid_t alt_uid);
#endif
static int  _mod_initialize(mod_t mod);
static int  _mod_init_list_safe(mod_t mod, void *arg);
static void _mod_destroy(mod_t mod);
static bool _mod_opts_ok(mod_t mod);
static int  _mod_print_info(mod_t mod);
static void _print_option_help(struct pdsh_module_option *p, int col);
static struct pdsh_module_option * _mod_find_opt(mod_t mod, int opt);

/*
 *  Static list of loaded modules
 */
static List module_list;
static bool initialized = false;

int
mod_init(void)
{
    if (!initialized) {
        if (!(module_list = list_create((ListDelF) _mod_destroy))) {
            err("Unable to create module list\n");
            return -1;
        }
        initialized = true;
        return 0;
    } else
        return 0;
}

int
mod_exit(void)
{
    if (!initialized)
        return 0;

    /*
     *  list_destroy() will call module destructor on each 
     *    element in list
     */
    list_destroy(module_list);
    
    return 0;
}

hostlist_t
_mod_read_wcoll(mod_t mod, opt_t *pdsh_opts)
{
    if (mod->pmod->mod_ops && mod->pmod->mod_ops->read_wcoll)
        return (*mod->pmod->mod_ops->read_wcoll) (pdsh_opts);
    return 0;
}


int
_mod_postop(mod_t mod, opt_t *pdsh_opts)
{
    if (mod->pmod->mod_ops && mod->pmod->mod_ops->postop)
        return (*mod->pmod->mod_ops->postop) (pdsh_opts);

    return 0;
}


/*
 *  Like list_next (i), but skip over inactive modules
 */
static mod_t _mod_next_active (ListIterator i)
{
    mod_t mod;

    while ((mod = list_next (i))) {
        if (mod->initialized)
            return (mod);
    }

    return (NULL);
}


/*
 *  Call any "read wcoll" functions exported by modules. The module
 *    is responsible for deciding when to generate a new wcoll, 
 *    append to existing wcoll, etc. (presumably based on the contents
 *    of opt)
 *
 *  This function appends to wcoll any new hosts returned from the
 *    module specific read_wcoll() functions. 
 *
 *  Returns -1 on error, 0 for success.
 */  
int
mod_read_wcoll(opt_t *opt)
{
    mod_t mod;
    ListIterator module_itr;

    if (!initialized)
        mod_init();

    if (!(module_itr = list_iterator_create(module_list))) {
        err("Unable to create module list iterator\n");
        return -1;
    }

    while ((mod = _mod_next_active (module_itr))) {
        hostlist_t hl = NULL;

        if (!(hl = _mod_read_wcoll(mod, opt)))
            continue;

        if (opt->wcoll != NULL) {
            hostlist_push_list(opt->wcoll, hl);
            hostlist_destroy(hl);
        } else
            opt->wcoll = hl;
    }

    list_iterator_destroy(module_itr);

    return 0;
}

int
mod_postop(opt_t *pdsh_opts)
{
    mod_t mod;
    int errors = 0;
    ListIterator module_itr;

    if (!initialized)
        mod_init();

    if (!(module_itr = list_iterator_create(module_list))) {
        err("Unable to create module list iterator\n");
        return 1;
    }

    while ((mod = _mod_next_active (module_itr)))
        errors += _mod_postop(mod, pdsh_opts);

    list_iterator_destroy(module_itr);

    return errors;
}


mod_t
mod_create(void)
{
    mod_t mod = Malloc(sizeof(*mod));
#if !STATIC_MODULES
    mod->handle = NULL;
#endif
    mod->filename = NULL;

    mod->priority = DEFAULT_MODULE_PRIORITY;
    mod->initialized = 0;
    assert(mod->magic = MOD_MAGIC);
    return mod;
}

static void
_mod_destroy(mod_t mod)
{
    assert(mod->magic == MOD_MAGIC);

    /* must assert mod->mod, loading of module may have failed */
    if (mod->pmod) {
        mod->pmod->type = NULL;
        mod->pmod->name = NULL;

        /*
         *  Only run exit function if module was initialized
         */
        if (mod->initialized &&
            mod->pmod->mod_ops && mod->pmod->mod_ops->exit)
            (*mod->pmod->mod_ops->exit)();
    }

    if (mod->filename)
        Free((void **) &mod->filename);
    
#if !STATIC_MODULES
#  if !PREVENT_DLCLOSE_BUG
    if (mod->handle)
        dlclose(mod->handle);
#  endif
#endif

    assert(mod->magic = ~MOD_MAGIC);
    Free((void **) &mod);

    return;
}

static bool
_mod_opts_ok(mod_t mod)
{
    if (!opt_register(mod->pmod->opt_table))
        return false;

    return true;
}

static int
_cmp_f (mod_t x, mod_t y)
{
    if (x->priority == y->priority)
        return strcmp (x->pmod->name, y->pmod->name);
    return (y->priority - x->priority);
}


static int
_mod_find_misc (mod_t mod, const char *name)
{
    if (strcmp (mod->pmod->type, "misc") != 0)
        return 0;
    if (strcmp (mod->pmod->name, name) != 0)
        return 0;
    return 1;
}

static int
_mod_initialize_by_name (char *name, List l)
{
    mod_t mod = list_find_first (l, (ListFindF) _mod_find_misc, name);
    if (mod != NULL && _mod_initialize (mod) < 0)
        err("%p: Warning: Failed to initialize requested module \"%s/%s\"\n",
                mod->pmod->type, mod->pmod->name);
    return (0);
}


static int _mod_initialize_modules_by_name (char *names, List m)
{
    List l;

    if (names == NULL)
        return (0);

    l = list_split (",", names);
    list_for_each (l, (ListForF) _mod_initialize_by_name, m);
    return (0);
}

int mod_load_modules(const char *dir, opt_t *opt)
{
    int rc = 0;
#if STATIC_MODULES
    rc = _mod_load_static_modules();
#else
    rc = _mod_load_dynamic_modules(dir, opt);
#endif

    list_sort(module_list, (ListCmpF) _cmp_f);

    /*
     *  Initialize misc modules by name
     */
    _mod_initialize_modules_by_name (opt->misc_modules, module_list);

    /*
     *  Initialize remaining modules in modules_list:
     */
    list_for_each (module_list, (ListForF) _mod_init_list_safe, NULL);

    return(rc);
}


/*
 *  Print all options from module option table 'p,' aligning description
 *    with column 'col'
 */
static void 
_print_option_help(struct pdsh_module_option *p, int col)
{
    char buf[81];

    assert(p != NULL);

    snprintf(buf, 81, "-%c %-*s %s\n", p->opt, col - 4, 
             (p->arginfo ? p->arginfo : ""), p->descr);

    err("%s", buf);
}

void
mod_print_options(mod_t mod, int col)
{
    struct pdsh_module_option *p;

    assert(mod != NULL);
    assert(mod->pmod != NULL);

    p = mod->pmod->opt_table;
    if (!p || !p->opt)
        return;
    /* 
     * out("%s/%s Options:\n", mod->pmod->type, mod->pmod->name);
     */
    for (p = mod->pmod->opt_table; p && (p->opt != 0); p++) 
        _print_option_help(p, col);
        
}

/*
 *  Print to stdout information stanza for module "mod"
 */
static int 
_mod_print_info(mod_t mod)
{
    if (mod == NULL) 
        return 0;

    out("Module: %s/%s\n",    mod->pmod->type, mod->pmod->name); 
    out("Author: %s\n",       mod->pmod->author ? mod->pmod->author : "???");
    out("Descr:  %s\n",       mod->pmod->descr ? mod->pmod->descr : "???");
    out("Active: %s\n",       mod->initialized ? "yes" : "no");

    if (mod->pmod->opt_table && mod->pmod->opt_table->opt) {
        out("Options:\n");
        mod_print_options(mod, 18);
    }

    out("\n");

    return 0;
}

static int _opt_print(mod_t mod, int *col)
{
    if (mod->initialized)
        mod_print_options(mod, *col);
    return 0;
}


void mod_print_all_options(int col) 
{
    list_for_each(module_list, (ListForF) _opt_print, &col);
}


static int
_cmp_type(mod_t mod, char *type)
{
    return (strcmp(mod->pmod->type, type) == 0);
}

int
mod_count(char *type)
{
    int i = 0;
    ListIterator module_itr;

    assert(module_list != NULL);

    if (type == NULL)
        return list_count(module_list);

    if (!(module_itr = list_iterator_create(module_list))) {
        err("Unable to create module list iterator\n");
        return -1;
    }

    while (list_find(module_itr, (ListFindF) _cmp_type, type)) 
        i++;

    list_iterator_destroy(module_itr);

    return i;
}

static List
_mod_get_module_names(char *type, int get_active)
{
    List l;
    mod_t mod;
    ListIterator module_itr;

    assert(module_list != NULL);

    l = list_create(NULL);

    if (!(module_itr = list_iterator_create(module_list))) {
        err("Unable to create module list iterator\n");
        list_destroy(l);
        return NULL;
    }

    while ((mod = (type ? list_find(module_itr, (ListFindF) _cmp_type, type)
                        : list_next(module_itr)))) {
        /*
         *  Push active (initialized) modules onto list if get_active
         *   is true, otherwise push inactive (!initialized) modules:
         */
        if (!get_active == !mod->initialized)
            list_push(l, mod->pmod->name);
    }

    list_iterator_destroy(module_itr);

    return l;
}

List mod_get_module_names (char *type)
{
    return _mod_get_module_names (type, 1);
}

List mod_get_uninitialized_module_names (char *type)
{
    return _mod_get_module_names (type, 0);
}


void
mod_list_module_info(void)
{
    int nmodules = list_count(module_list);
    out("%d module%s loaded:\n\n", nmodules, (nmodules > 1 ? "s" : ""));

    if (nmodules == 0)
        return;

    list_for_each(module_list, (ListForF) _mod_print_info, NULL);
}


static int
_mod_description_match (mod_t m, const char *type, const char *name)
{
    if (  (strcmp(m->pmod->type, type) == 0)
       && (strcmp(m->pmod->name, name) == 0) )
        return (1);
    return (0);
}


mod_t
mod_get_module(const char *type, const char *name)
{
    mod_t mod;
    ListIterator module_itr;

    assert(type != NULL);
    assert(name != NULL);

    if (!(module_itr = list_iterator_create(module_list))) {
        err("Unable to create module list iterator\n");
        return NULL;
    }

    while ((mod = list_next(module_itr))) {
        if (_mod_description_match (mod, type, name))
            break;
    }

    list_iterator_destroy(module_itr);

    return mod;
}

char *
mod_get_name(mod_t mod)
{
    assert(mod != NULL);
    assert(mod->pmod != NULL);

    return mod->pmod->name;
}

char *
mod_get_type(mod_t mod)
{
    assert(mod != NULL);
    assert(mod->pmod != NULL);

    return mod->pmod->type;
}

RcmdInitF
mod_get_rcmd_init(mod_t mod) {

    assert(mod != NULL);
    assert(mod->pmod != NULL);

    if (mod->pmod->rcmd_ops && mod->pmod->rcmd_ops->rcmd_init)
        return mod->pmod->rcmd_ops->rcmd_init;
    else
        return NULL;
}

RcmdSigF
mod_get_rcmd_signal(mod_t mod) {

    assert(mod != NULL);
    assert(mod->pmod != NULL);

    if (mod->pmod->rcmd_ops && mod->pmod->rcmd_ops->rcmd_signal)
        return mod->pmod->rcmd_ops->rcmd_signal;
    else
        return NULL;
}

RcmdF
mod_get_rcmd(mod_t mod) {

    assert(mod != NULL);
    assert(mod->pmod != NULL);

    if (mod->pmod->rcmd_ops && mod->pmod->rcmd_ops->rcmd)
        return mod->pmod->rcmd_ops->rcmd;
    else
        return NULL;
}

RcmdDestroyF
mod_get_rcmd_destroy (mod_t mod)
{
    assert (mod != NULL);
    assert (mod->pmod != NULL);
    
    if (mod->pmod->rcmd_ops && mod->pmod->rcmd_ops->rcmd_destroy)
        return mod->pmod->rcmd_ops->rcmd_destroy;
    else
        return NULL;
}


int 
mod_process_opt(opt_t *opt, int c, char *optarg)
{
    mod_t mod;
    struct pdsh_module_option *p = NULL;
    ListIterator module_itr;

    if (!(module_itr = list_iterator_create(module_list))) {
        err("Unable to create module list iterator\n");
        return -1;
    }

    while ((mod = _mod_next_active (module_itr))) {
        if ((p = _mod_find_opt(mod, c))) {
            list_iterator_destroy(module_itr);
            return p->f(opt, c, optarg);
        }
    }

    list_iterator_destroy(module_itr);

    return -1;
}


/*
 *  Return pointer to pdsh_module_option struct for option `opt'
 *    or NULL if no loaded module provides that option.
 */
static struct pdsh_module_option *
_mod_find_opt(mod_t mod, int opt)
{
  struct pdsh_module_option *p = mod->pmod->opt_table;
  for (p = mod->pmod->opt_table; p && (p->opt != 0); p++) 
      if (p->opt == opt) return p;
  return NULL;
}

static int 
_mod_delete (const char *type, const char *name)
{
    ListIterator i = list_iterator_create (module_list);
    mod_t m;

    while ((m = list_next (i))) {
        if (_mod_description_match (m, type, name))
            list_delete (i);
    }
    return (0);
}


static int _mod_register (mod_t mod, const char *name)
{
    mod_t prev;
    /* 
     *  Must have atleast a name and type 
     */
    if (!mod->pmod->type || !mod->pmod->name) {
        err("%p:[%s] type or name not specified in module\n", name);
        return -1;
    }

    /*
     *  Check for existing module of the same type and name
     *   Delete previous module if its priority is higher.
     */
    if ((prev = mod_get_module (mod->pmod->type, mod->pmod->name))) {
        err("%p: %s: [%s/%s] already loaded from [%s]\n", 
                mod->filename, mod->pmod->type, mod->pmod->name, 
                prev->filename);
        if (mod->priority > prev->priority)
            _mod_delete (mod->pmod->type, mod->pmod->name);
        else
            return (-1);
    }

    /* 
     * Continue with module loading only if personality acceptable 
     */
    if (!(mod->pmod->personality & pdsh_personality()))
        return -1;

    list_prepend(module_list, mod);

    return 0;
}

static int _mod_initialize (mod_t mod)
{
    if (!_mod_opts_ok(mod))
        return -1;

    if (mod->pmod->mod_ops && 
        mod->pmod->mod_ops->init && 
        ((*mod->pmod->mod_ops->init)() < 0)) {
        err("%p: error: %s/%s failed to initialize.\n", 
            mod->pmod->type, mod->pmod->name);
        return -1;
    }

    mod->initialized = 1;

    return 0;
}

/*
 *  Version of _mod_initialize that always returns zero
 *   for use with list_for_each.
 */
static int _mod_init_list_safe (mod_t mod, void *arg)
{
    _mod_initialize (mod);
    return 0;
}


#if STATIC_MODULES
/*
 *   Set pdsh module pointer (pmod) to point to address from
 *    statically defined external static_mods array.
 */
static int
_mod_load_static(int idx)
{
    mod_t mod = mod_create();

    mod->pmod = static_mods[idx];
    mod->priority = *priority[idx];
    mod->filename = Strdup("static");

    _mod_register(mod, static_mod_names[idx]);

    return 0;
}

/*
 *  Load all statically defined modules from internal static_mods array
 */
static int 
_mod_load_static_modules(void)
{
    int i = 0;

    while (static_mods[i] != NULL) {
        if (_mod_load_static(i++) < 0)
            continue;
    }

    return 0;
}


#else /* !STATIC_MODULES */

/*
 *  Load a single module from file `fq_path' and append to module_list.
 */
static int
_mod_load_dynamic(const char *fq_path)
{
    mod_t mod = NULL;
    int *priority;
    assert(fq_path != NULL);

    mod = mod_create();

    if (!(mod->handle = dlopen(fq_path,RTLD_LAZY)))
        goto fail;

    mod->filename = basename(fq_path);

    if (_is_loaded(mod->filename)) {
        /* Module already loaded. This is OK, no need for
         *   error message. (Could have already opened a .la and
         *   we are now opening the corresponding .so 
         */
        goto fail;
    }
  
    /* load all module info from the pdsh_module structure */
    if (!(mod->pmod = dlsym(mod->handle, "pdsh_module_info"))) {
        err("%p:[%s] can't resolve pdsh module\n", mod->filename);
        goto fail;
    }

    if ((priority = dlsym(mod->handle, "pdsh_module_priority"))) 
        mod->priority = *priority;

    if (_mod_register(mod, mod->filename) < 0)
        goto fail;

    return 0;

 fail_libtool_broken:
    /*
     * Avoid dlclose() of invalid handle
     */
    mod->handle = NULL;

 fail:
    _mod_destroy(mod);
    return -1;
}

static int  
_pdsh_owner(const char *pdsh_path, uid_t *pdsh_uid)
{
    struct stat st;
  
    if (stat (pdsh_path, &st) < 0) {
        err ("%p: Unable to determine ownership of pdsh binary: %m\n");
        return -1;
    }

    *pdsh_uid = st.st_uid;

    return 0;
}

static int
_mod_load_dynamic_modules(const char *dir, opt_t *pdsh_opts)
{
    DIR           *dirp   = NULL;
    struct dirent *entry  = NULL;
    char           path[MAXPATHLEN + 1];
    char           *p;
    int            count = 0;
    uid_t          pdsh_owner = 0;

    assert(dir != NULL);
    assert(*dir != '\0');

    if (!initialized) 
        mod_init();

    if (_pdsh_owner(pdsh_opts->local_program_path, &pdsh_owner) < 0)
        return -1;

    if (!_path_permissions_ok(dir, pdsh_owner)) 
        return -1;

    if (!(dirp = opendir(dir)))
        return -1;

    strncpy(path, dir, MAXPATHLEN);
    p = path + strlen(dir);
    *(p++) = '/';

    while ((entry = readdir(dirp))) {
        struct stat st;

        strcpy(p, entry->d_name);

        if (stat(path, &st) < 0)
            continue; 
        if (!S_ISREG(st.st_mode))
            continue;

        /*
         *  Do not load modules that could have been altered by
         *   a user other than root or the current user or the user
         *   owning the pdsh executable. Otherwise pdsh could execute 
         *   arbitrary code.
         */
        if (  (st.st_uid != 0) && (st.st_uid != getuid())
           && (st.st_uid != pdsh_owner)) {
            err ("%p: skipping insecure module \"%s\" (check owner)\n", path);
            continue;
        }
        if (st.st_mode & S_IWOTH) {
            err ("%p: skipping insecure module \"%s\" (check perms)\n", path);
            continue;
        }
        
        if (_mod_load_dynamic(path) < 0) 
            continue;

        count++;
    }

    if (closedir(dirp) < 0)
        err("%p: error closing %s: %m", dir);

    if (count == 0)
        errx("%p: no modules found\n"); 

    return 0;
}


static int _cmp_filenames(mod_t mod, char *filename)
{
    return (strcmp(mod->filename, filename) == 0);
}

static int
_is_loaded(char *filename)
{
    if (list_find_first(module_list, (ListFindF) _cmp_filenames, filename))
        return 1;

    return 0;
}

static char * 
_perm_error_string (perm_error_t error)
{
    switch (error) {
        case DIR_OK: 
            return ("Permissions are valid");
        case DIR_NOT_DIRECTORY:
            return ("Not a directory");
        case DIR_BAD_OWNER:
            return ("Owner not root, current uid, or pdsh executable owner");
        case DIR_WORLD_WRITABLE:
            return ("World writable and sticky bit is not set");
        default:
            break;
    }

    return ("Unspecified error");
}


/*
 *  Return permissions error if stat buffer shows any of the below
 *   1. This is not a directory.
 *   2. Ownership something other than root, the current uid, or the
 *      same ownership as the pdsh executable.
 *   3. Permissions are world writable and sticky bit is not set.
 */
static perm_error_t
_dir_permission_error(struct stat *st, uid_t alt_uid)
{
    if (!S_ISDIR(st->st_mode))
        return DIR_NOT_DIRECTORY;
    if (  (st->st_uid != 0) && (st->st_uid != getuid()) 
       && (st->st_uid != alt_uid)) 
        return DIR_BAD_OWNER;
    if ((st->st_mode & S_IWOTH) && !(st->st_mode & S_ISVTX))
        return DIR_WORLD_WRITABLE;
    return DIR_OK;
}

/*
 *  Temprarily chdir() to path and use getcwd to return real patch
 *   to caller. 
 */
static char *
_get_dir_name (const char *path, char *buf, size_t len)
{
    int pathlen = 256;
    char * orig_path = Malloc (pathlen * sizeof (char));

    while (!getcwd (orig_path, pathlen) && (pathlen < MAXPATHLEN*2))
        Realloc ((void **) &orig_path, pathlen*=2 * sizeof (char)); 

    if (chdir (path) < 0)
        errx ("Unable to chdir() to %s: %m", path);

    if (!getcwd (buf, len))
        errx ("Unable to get working directory for module path: %s\n",
                path);

    if (chdir (orig_path) < 0)
        err ("Unable to return to original working directory: %s: %m\n",
                orig_path);

    Free ((void **) &orig_path);

    return (buf);
}

/*
 *  Returns true if, for the directory "dir" and all of its parent 
 *    directories,  the following are true:
 *    - ownership is root or the calling user (as returned by getuid())
 *        or same ownership as the pdsh or pdcp binary.
 *    - directory has user write permission only
 *
 *  Returns false if one of the assertions above are false for any
 *    directory in the tree.
 *
 */
static bool
_path_permissions_ok(const char *dir, uid_t pdsh_owner)
{
    struct stat st;
    char dirbuf[MAXPATHLEN + 1];
    dev_t rootdev;
    ino_t rootino;
    perm_error_t error;
    int pos = 0;

    assert(dir != NULL);

    if (stat("/", &st) < 0) {
        err("%p: Can't stat root directory: %m\n");
        return false;
    }

    rootdev = st.st_dev;
    rootino = st.st_ino;

    strncpy(dirbuf, dir, MAXPATHLEN);
    dirbuf[MAXPATHLEN] = '\0';
    pos = strlen(dirbuf);

    do {

        if (stat(dirbuf, &st) < 0) {
            err("%p: Can't stat \"%s\": %m\n", dir);
            return false;
        }

        if ((error = _dir_permission_error(&st, pdsh_owner)) != DIR_OK) {
            char buf [MAXPATHLEN];
            err("%p: module path \"%s\" insecure.\n", dir);
            err("%p: \"%s\": %s\n", 
                _get_dir_name (dirbuf, buf, MAXPATHLEN), 
                _perm_error_string (error)); 
            return false;
        }

        /*  Check for impending overflow
         */
        if (pos > MAXPATHLEN - 3) {
            err("%p :-( Path too long while checking permissions\n");
            return false;
        }

        /* Check parent
         */
        strncpy(&dirbuf[pos],  "/..", 4);
        pos+=3;

    } while ( !((st.st_ino == rootino) && (st.st_dev == rootdev)) );

    return true;
}

#endif /* !STATIC_MODULES */

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

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
#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <assert.h>
#include <string.h>

#include "ltdl.h"

#include "mod.h"

#include "err.h"
#include "xmalloc.h"
#include "xstring.h"
#include "hostlist.h"
#include "list.h"


/*
 *  Components of a module.
 */
struct module_components {
#ifndef NDEBUG
#define MOD_MAGIC 0xc0c0b0b
	int magic;
#endif
	lt_dlhandle handle;
	char *filename;
	char *type;
	char *name;
	char *descr;
	char *author;

	struct pdsh_module_option     *opt_table;
    struct pdsh_module_operations *ops;
};

/* 
 *  Static prototypes:
 */
static int  _is_loaded(char *filename); 
static int  _mod_load(const char *filename);
static void _mod_destroy(mod_t mod);
static bool _mod_opts_ok(mod_t mod);
static int  _mod_print_info(mod_t mod);
static void _print_option_help(struct pdsh_module_option *p, int col);
static bool _path_permissions_ok(const char *dir);
static bool _dir_ok(struct stat *);
static struct pdsh_module_option * _mod_find_opt(mod_t mod, int opt);

/*
 *  Static list of loaded modules and persistent iterator
 */
static List module_list;
static ListIterator module_itr;
static bool initialized = false;

int
mod_init(void)
{
	if (!initialized) {
        if ( !(module_list = list_create((ListDelF) _mod_destroy))
           || !(module_itr = list_iterator_create(module_list))  ) {
            err("Unable to create module list and iterator\n");
            return -1;
        }
        initialized = true;
		return lt_dlinit();
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
    list_iterator_destroy(module_itr);
    list_destroy(module_list);
    
	return lt_dlexit(); 
}

hostlist_t
_mod_read_wcoll(mod_t mod, opt_t *pdsh_opts)
{
    if (mod->ops->read_wcoll)
        return (*mod->ops->read_wcoll) (pdsh_opts);
    return 0;
}


int
_mod_postop(mod_t mod, opt_t *pdsh_opts)
{
    if (mod->ops->postop)
        return (*mod->ops->postop) (pdsh_opts);
    return 0;
}


hostlist_t
mod_read_wcoll(opt_t *pdsh_opts)
{
    mod_t mod;
    hostlist_t wcoll = NULL;

    if (!initialized)
        mod_init();

    list_iterator_reset(module_itr);
    while ((mod = list_next(module_itr))) {
        if ((wcoll = _mod_read_wcoll(mod, pdsh_opts)))
            break;
    }
    return wcoll;
}

int
mod_postop(opt_t *pdsh_opts)
{
    mod_t mod;
    int errors = 0;

    if (!initialized)
        mod_init();

    list_iterator_reset(module_itr);
    while ((mod = list_next(module_itr)))
        errors += _mod_postop(mod, pdsh_opts);

    return errors;
}


mod_t
mod_create(void)
{
    mod_t mod = Malloc(sizeof(*mod));
	mod->handle = NULL;
	mod->type   = NULL;
	mod->name   = NULL;
	assert(mod->magic = MOD_MAGIC);

	return mod;
}

static void
_mod_destroy(mod_t mod)
{
	assert(mod->magic == MOD_MAGIC);

	if (mod->ops && mod->ops->exit)
		(*mod->ops->exit)();
    
    if (mod->filename)
        Free((void **) &mod->filename);

	mod->type = NULL;
	mod->name = NULL;

    if (mod->handle)
        lt_dlclose(mod->handle);

	assert(mod->magic = ~MOD_MAGIC);
	Free((void **) &mod);

	return;
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

static bool
_mod_opts_ok(mod_t mod)
{
	struct pdsh_module_option *p;
	for (p = mod->opt_table; p && (p->opt != 0); p++) {
		if (!opt_register(p)) 
			return false;
	}

	return true;
}

/*
 *  Load a single module from file `fq_path' and append to module_list.
 */
static int
_mod_load(const char *fq_path)
{
	mod_t mod = NULL;
	const lt_dlinfo *info;

	assert(fq_path != NULL);

	mod = mod_create();

	if (!(mod->handle = lt_dlopen(fq_path)))
		goto fail;

	if (!(info = lt_dlgetinfo(mod->handle))) 
        goto fail_libtool_broken;

	if (info->filename == NULL) 
		goto fail_libtool_broken;

	mod->filename = Strdup(info->filename);

	if (_is_loaded(mod->filename)) {
		err("%p: [%s] module already loaded\n", mod->filename);
		goto fail;
	}

	if (!(mod->type = lt_dlsym(mod->handle, "__module_type"))) {
		err("%p:[%s] can't resolve module type\n", mod->filename);
		goto fail;
	}

	if (!(mod->name = lt_dlsym(mod->handle, "__module_name"))) {
		err("%p:[%s] can't resolve module name\n", mod->filename);
		goto fail;
	}
    if (!(mod->ops = lt_dlsym(mod->handle, "pdsh_module_ops"))) {
        err("%p:[%s] can't resolve module operations\n", mod->filename);
        goto fail;
    }

    /*
     *  Optional exports
     */
	mod->descr       = lt_dlsym(mod->handle, "__module_descr");
	mod->author      = lt_dlsym(mod->handle, "__module_author");
	mod->opt_table   = lt_dlsym(mod->handle, "pdsh_module_options");


	if (!_mod_opts_ok(mod)) {
		err("failed to install module options for \"%s/%s\"\n", 
				mod->type, mod->name);
		goto fail;
	}

	if (mod->ops->init && ((*mod->ops->init)() < 0)) {
		err("%p: %s/%s module_init() failed\n", mod->type, mod->name);
		goto fail;
	}

	list_append(module_list, mod);

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


int 
mod_load_modules(const char *dir)
{
	int            retval = 0;
	DIR           *dirp   = NULL;
	struct dirent *entry  = NULL;
	char           path[MAXPATHLEN + 1];
	char           *p;

    assert(dir != NULL);
    assert(*dir != '\0');

    if (!initialized)
        mod_init();

	if (!_path_permissions_ok(dir)) 
		return -1;

	if (!(dirp = opendir(dir)))
        return -1;

	strncpy(path, dir, MAXPATHLEN);
	p = path + strlen(dir);
	*(p++) = '/';

	while ((entry = readdir(dirp))) {
		struct stat st;

		strcpy(p, entry->d_name);

        /*
         *  As an efficiency enhancement, only attempt to open 
         *    libtool ".la" files.
         */
        if (strcmp(&p[strlen(p) - 3], ".la") != 0)
            continue;

		if (stat(path, &st) < 0)
			continue;
		if (!S_ISREG(st.st_mode))
			continue;
		if (_mod_load(path) < 0) 
			continue;

		retval++;
	}

    if (closedir(dirp) < 0)
        err("%p: error closing %s: %m", dir);

	return 0;
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

    out("%s", buf);
}

void
mod_print_options(mod_t mod, int col)
{
	struct pdsh_module_option *p = mod->opt_table;
    if (!p || !p->opt)
        return;
    /* 
     * out("%s/%s Options:\n", mod->type, mod->name);
     */
	for (p = mod->opt_table; p && (p->opt != 0); p++) 
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
	out("Module: %s/%s\n",    mod->type, mod->name); 
	out("Author: %s\n",       mod->author ? mod->author : "???");
	out("Descr:  %s\n",       mod->descr ? mod->descr : "???");

	if (mod->opt_table) {
		out("Options:\n");
		mod_print_options(mod, 18);
	}

	out("\n");

    return 0;
}

void _opt_print(mod_t mod, int *col)
{
    mod_print_options(mod, *col);
}


void mod_print_all_options(int col)
{
    list_for_each(module_list, (ListForF) _opt_print, &col);
}


static int
_cmp_type(mod_t mod, char *type)
{
    return (strcmp(mod->type, type) == 0);
}

List
mod_get_module_names(char *type)
{
    List l;
    mod_t mod;

    assert(module_list != NULL);
    assert(type != NULL);
    assert(*type != '\0');

    l = list_create(NULL);

    list_iterator_reset(module_itr);
    while ((mod = list_find(module_itr, (ListFindF) _cmp_type, type)))
        list_push(l, mod->name);

    return l;
}

void
mod_list_module_info(void)
{
    int nmodules = list_count(module_list);
	if (nmodules == 0)
		return;
	out("%d module%s loaded:\n\n", nmodules, (nmodules > 1 ? "s" : ""));
	list_for_each(module_list, (ListForF) _mod_print_info, NULL);
}


void *
mod_get_sym(mod_t mod, const char *name)
{
	assert(mod != NULL);
	assert(mod->magic == MOD_MAGIC);
	assert(name != NULL);

	return (void *) lt_dlsym(mod->handle, name);
}


mod_t
mod_get_module(const char *type, const char *name)
{
    mod_t mod;

	assert(type != NULL);
	assert(name != NULL);

    list_iterator_reset(module_itr);
    while ((mod = list_next(module_itr))) {
		if ( (strncmp(mod->type, type, strlen(type)) == 0)
		   && (strncmp(mod->name, name, strlen(name)) == 0) )
			return mod;
	}
	return NULL;
}

char *
mod_get_name(mod_t mod)
{
    return mod->name;
}

char *
mod_get_type(mod_t mod)
{
    return mod->type;
}

int 
mod_process_opt(opt_t *opt, int c, char *optarg)
{
    mod_t mod;
	struct pdsh_module_option *p = NULL;

    list_iterator_reset(module_itr);
    while ((mod = list_next(module_itr))) {
        if ((p = _mod_find_opt(mod, c)))
            return p->f(opt, c, optarg);
    }
	return -1;
}


/*
 *  Return pointer to pdsh_module_option struct for option `opt'
 *    or NULL if no loaded module provides that option.
 */
static struct pdsh_module_option *
_mod_find_opt(mod_t mod, int opt)
{
	struct pdsh_module_option *p = mod->opt_table;
	for (p = mod->opt_table; p && (p->opt != 0); p++) 
		if (p->opt == opt) return p;
	return NULL;
}


/*
 *  Return true if stat struct show ownership of root or calling user,
 *    and write permissions for user and group only.
 */
static bool
_dir_ok(struct stat *st)
{
    if ((st->st_uid != 0) && (st->st_uid != getuid())) 
        return false;
    if ((st->st_mode & S_IWOTH) /* || (st->st_mode & S_IWGRP) */) 
        return false;
    return true;
}


/*
 *  Returns true if, for the directory "dir" and all of its parent 
 *    directories,  the following are true:
 *    - ownership is root or the calling user (as returned by getuid())
 *    - directory has user write permission only
 *
 *  Returns false if one of the assertions above are false for any
 *    directory in the tree.
 *
 */
static bool
_path_permissions_ok(const char *dir)
{
    struct stat st;
    char dirbuf[MAXPATHLEN + 1];
    dev_t rootdev;
    ino_t rootino;
    int pos = 0;

    assert(dir != NULL);

    if (lstat("/", &st) < 0) {
        err("%p: Can't stat root directory: %m\n");
        return false;
    }

    rootdev = st.st_dev;
    rootino = st.st_ino;

    strncpy(dirbuf, dir, MAXPATHLEN);
    dirbuf[MAXPATHLEN] = '\0';
    pos = strlen(dirbuf);

    do {

        if (lstat(dirbuf, &st) < 0) {
            err("%p: Can't stat \"%s\": %m\n", dir);
            return false;
        }

        if (!_dir_ok(&st)) {
            err("%p: module path \"%s\" insecure.\n", dir);
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

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

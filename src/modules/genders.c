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
#  include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <genders.h>

#include "src/common/hostlist.h"
#include "src/common/err.h"
#include "src/common/xmalloc.h"
#include "src/common/split.h"
#include "src/common/xstring.h"
#include "src/pdsh/mod.h"
#include "src/pdsh/rcmd.h"

#define ALL_NODES NULL

#ifndef GENDERS_ALTNAME_ATTRIBUTE
#  define GENDERS_ALTNAME_ATTRIBUTE   "altname"
#endif

#if STATIC_MODULES
#  define pdsh_module_info genders_module_info
#  define pdsh_module_priority genders_module_priority
#endif    

int pdsh_module_priority = DEFAULT_MODULE_PRIORITY;


/*
 *  Static genders module interface routines:
 */
static hostlist_t genders_wcoll(opt_t *pdsh_opts);
static int        genders_process_opt(opt_t *, int, char *);
static int        genders_init(void);
static int        genders_fini(void);
static int        genders_postop(opt_t *);


#if !GENDERS_G_ONLY

static bool allnodes   = false;
static bool opt_i      = false;
#endif /* !GENDERS_G_ONLY */
static bool genders_opt_invoked = false;
static bool generate_altnames =   false;

static genders_t gh    = NULL;
static char *gfile     = NULL;
static List attrlist   = NULL;
static List excllist   = NULL;

/* 
 * Export pdsh module operations structure
 */
struct pdsh_module_operations genders_module_ops = {
    (ModInitF)       genders_init, 
    (ModExitF)       genders_fini, 
    (ModReadWcollF)  genders_wcoll, 
    (ModPostOpF)     genders_postop,
};

/* 
 * Export rcmd module operations
 */
struct pdsh_rcmd_operations genders_rcmd_ops = {
    (RcmdInitF)  NULL,
    (RcmdSigF)   NULL,
    (RcmdF)      NULL,
};

/* 
 * Export module options
 */
struct pdsh_module_option genders_module_options[] = 
 { 
   { 'g', "query,...", 
     "target nodes using genders query",
     DSH | PCP, (optFunc) genders_process_opt 
   },
   { 'X', "query,...", 
     "exclude nodes using genders query",
     DSH | PCP, (optFunc) genders_process_opt
   },
   { 'F', "file",          "use alternate genders file `file'",
     DSH | PCP, (optFunc) genders_process_opt
   },
#if !GENDERS_G_ONLY
   { 'i', NULL, "request alternate or canonical hostnames if applicable",
     DSH | PCP, (optFunc) genders_process_opt
   },
   { 'a', NULL, "target all nodes except those with \"pdsh_all_skip\" attribute", 
     DSH | PCP, (optFunc) genders_process_opt 
   },
   { 'A', NULL, "target all nodes listed in genders database",
     DSH | PCP, (optFunc) genders_process_opt
   },
#endif /* !GENDERS_G_ONLY */
   PDSH_OPT_TABLE_END
 };

/* 
 * Genders module info 
 */
struct pdsh_module pdsh_module_info = {
    "misc",
#if GENDERS_G_ONLY
    "genders-g",
#else
    "genders",
#endif /* GENDERS_G_ONLY */
    "Jim Garlick <garlick@llnl.gov>",
    "target nodes using libgenders and genders attributes",
    DSH | PCP, 

    &genders_module_ops,
    &genders_rcmd_ops,
    &genders_module_options[0],
};

/*
 * Static prototypes
 */
static genders_t  _handle_create();
static hostlist_t _genders_to_altnames(genders_t g, hostlist_t hl);
static hostlist_t _read_genders(List l);
static hostlist_t _read_genders_attr(char *query);
static void       _genders_opt_verify(opt_t *opt);
static int        _delete_all (hostlist_t hl, hostlist_t dl);
static int        register_genders_rcmd_types (opt_t *opt);


/*
 *  Functions:
 */
int
genders_process_opt(opt_t *pdsh_opts, int opt, char *arg)
{
    switch (opt) {
#if !GENDERS_G_ONLY
    case 'a':  
        /* For -a, exclude nodes with "pdsh_all_skip" */ 
        excllist = list_split_append (excllist, ",", "pdsh_all_skip");
    case 'A':
        allnodes = true;
        break;
    case 'i':
        opt_i    = true;
        break;
#endif /* !GENDERS_G_ONLY */
    case 'g':
        attrlist = list_split_append (attrlist, ",", arg);
        break;
    case 'X':
        excllist = list_split_append (excllist, ",", arg);
        break;
    case 'F':
        gfile = Strdup (arg);
        break;
    default:
        err("%p: genders_process_opt: invalid option `%c'\n", opt);
        return -1;
        break;
    }
    genders_opt_invoked = true;
    return 0;
}

static int
genders_init(void)
{
    return 0;
}

static int
genders_fini(void)
{
    if (attrlist)
        list_destroy (attrlist);

    if (excllist)
        list_destroy (excllist);

    if ((gh != NULL) && (genders_handle_destroy(gh) < 0))
        errx("%p: Error destroying genders handle: %s\n", genders_errormsg(gh));

    return 0;
}

static hostlist_t 
genders_wcoll(opt_t *opt)
{
    _genders_opt_verify(opt);

    if (opt->wcoll)
        return (NULL);

#if GENDERS_G_ONLY
    if (!attrlist)
        return NULL;
#else
    if (!allnodes && !attrlist)
        return NULL;
#endif /* !GENDERS_G_ONLY */

    if (gh == NULL)
        gh = _handle_create();

    generate_altnames = true;
    return _read_genders(attrlist);
}

static hostlist_t hostlist_intersect (hostlist_t h1, hostlist_t h2)
{
    char *host;
    hostlist_t r = hostlist_create (NULL);
    hostlist_iterator_t i = hostlist_iterator_create (h1);

    while ((host = hostlist_next (i))) {
        if (hostlist_find (h2, host) >= 0) {
            hostlist_push_host (r, host);
	}
        free (host);
    }
    hostlist_iterator_destroy (i);
    return (r);
}

static hostlist_t genders_query_with_altnames (char *query)
{
    hostlist_t r = _read_genders_attr (query);
    hostlist_t altlist = _genders_to_altnames (gh, r);
    hostlist_push_list (r, altlist);
    hostlist_destroy (altlist);
    return (r);
}

/*
 *  Filter hostlist hl on a list of genders queries in query_list.
 *   Multiple queries are ORed together, so a given host must only
 *   match a single query.
 */
static hostlist_t genders_filter (hostlist_t hl, List query_list)
{
    char *s;
    ListIterator i;
    hostlist_t result;

    if ((query_list == NULL) || (list_count (query_list) == 0))
        return hl;

    if ((i = list_iterator_create (query_list)) == NULL) {
        err ("%p: genders: failed to create list or hostlist iterator\n");
        return hl;
    }

    /*
     *  Result is the union of the intersection of each genders query
     *   with the incoming hostlist [hl]
     */
    result = hostlist_create (NULL);
    while ((s = list_next (i))) {
        hostlist_t ghl = genders_query_with_altnames (s);
        hostlist_t r = hostlist_intersect (hl, ghl);
        hostlist_destroy (ghl);

        hostlist_push_list (result, r);
    }
    list_iterator_destroy (i);
    hostlist_uniq (result);
    hostlist_destroy (hl);
    return (result);
}

static int
genders_postop(opt_t *opt)
{
    hostlist_t hl = NULL;

    if (!opt->wcoll)
        return (0);

    if (gh == NULL)
        gh = _handle_create();

    if (attrlist)
        opt->wcoll = genders_filter (opt->wcoll, attrlist);

    if (excllist && (hl = _read_genders (excllist))) {
        hostlist_t altlist = _genders_to_altnames (gh, hl);
        _delete_all (opt->wcoll, hl);
        _delete_all (opt->wcoll, altlist);

        hostlist_destroy (altlist);
        hostlist_destroy (hl);
    }

#if !GENDERS_G_ONLY
    /*
     *  Genders module returns altnames by default, but only
     *   when genders fills in wcoll or with -i, not when filtering via
     *   -g or -X.
     */
    if ((generate_altnames && !opt_i) || (!generate_altnames && opt_i)) {
        hostlist_t hl = opt->wcoll;
        opt->wcoll = _genders_to_altnames(gh, hl);
        hostlist_destroy(hl);
    }
#endif

    /*
     *  Apply any pdsh_rcmd_type setting from genders file
     *   based on current opt->wcoll.
     */
    register_genders_rcmd_types (opt);

    return (0);
}


/* 
 * Verify options passed to this module
 */
static void
_genders_opt_verify(opt_t *opt)
{
#if !GENDERS_G_ONLY
/*    if (altnames && !allnodes && (gend_attr == NULL)) {
 *       err("%p: Warning: Ignoring -i without -a or -g\n");
 *       altnames = false;
 *   }
 */
    if (allnodes && (attrlist != NULL))
        errx("%p: Do not specify -A or -a with -g\n");
#endif /* !GENDERS_G_ONLY */
    return;
}

static int
_maxnamelen (genders_t g)
{
    int maxvallen, maxnodelen;
    if ((maxvallen = genders_getmaxvallen(g)) < 0)
        errx("%p: genders: getmaxvallen: %s\n", genders_errormsg(g));
    if ((maxnodelen = genders_getmaxvallen(g)) < 0)
        errx("%p: genders: getmaxnodelen: %s\n", genders_errormsg(g));

    return (maxvallen > maxnodelen ? maxvallen : maxnodelen);
}


static hostlist_t
_genders_to_altnames(genders_t g, hostlist_t hl)
{
    hostlist_t retlist = NULL;
    hostlist_iterator_t i = NULL;
    int  maxlen = 0;
    char *altname = NULL;
    char *altattr = GENDERS_ALTNAME_ATTRIBUTE;
    char *host    = NULL;
    int  rc;

    if ((retlist = hostlist_create(NULL)) == NULL)
        errx("%p: genders: hostlist_create: %m\n");

    maxlen = _maxnamelen (g);
    altname = Malloc (maxlen + 1);

    if ((i = hostlist_iterator_create(hl)) == NULL)
        errx("%p: genders: hostlist_iterator_create: %m");

    while ((host = hostlist_next (i))) {
        memset(altname, '\0', maxlen);

        rc = genders_testattr(g, host, altattr, altname, maxlen + 1);

        /*
         *  If node not found, attempt to lookup canonical name via
         *   altername name.
         */
        if ((rc < 0) && (genders_errnum(g) == GENDERS_ERR_NOTFOUND)) 
            rc = genders_getnodes (g, &altname, 1, altattr, host);

        if (hostlist_push_host(retlist, (rc > 0 ? altname : host)) <= 0)
            err("%p: genders: warning: target `%s' not parsed: %m", host);

        free(host);
    }

    hostlist_iterator_destroy(i);

    Free((void **) &altname);

    return (retlist);
}

static hostlist_t 
_genders_to_hostlist(genders_t gh, char **nodes, int nnodes)
{
    hostlist_t hl = NULL;
    int i;

    if ((hl = hostlist_create(NULL)) == NULL)
        errx("%p: genders: hostlist_create failed: %m");

    for (i = 0; i < nnodes; i++) {
        if (hostlist_push_host(hl, nodes[i]) <= 0)
            err("%p: warning: target `%s' not parsed: %m\n", nodes[i]);
    }

    hostlist_uniq(hl);

    return hl;
}

static char * genders_filename_create (char *file)
{
    char *genders_file;
    const char *gdir = getenv ("PDSH_GENDERS_DIR");

    /*
     *  Return a copy of filename if user specified an
     *   absolute or relative path:
     */
    if (file[0] == '/' || file[0] == '.')
        return Strdup (file);

    /*
     *  Otherwise, append filename to
     *   PDSH_GENDERS_DIR (or /etc by default)
     */
    genders_file = gdir ? Strdup (gdir) : Strdup ("/etc");
    xstrcatchar (&genders_file, '/');
    xstrcat (&genders_file, file);

    return (genders_file);
}

static genders_t _handle_create()
{
    char *gfile_env;
    char *genders_file = NULL;
    genders_t gh = NULL;

    if ((gh = genders_handle_create()) == NULL)
        errx("%p: Unable to create genders handle: %m\n");

    if (gfile)
        genders_file = genders_filename_create (gfile);
    else if ((gfile_env = getenv ("PDSH_GENDERS_FILE")))
        genders_file = genders_filename_create (gfile_env);
    else
        genders_file = genders_filename_create ("genders");

    /*
     *  Only exit on error from genders_load_data() if an genders
     *   module option was explicitly invoked:
     */
    if ((genders_load_data(gh, genders_file) < 0) && genders_opt_invoked)
        errx("%p: %s: %s\n", genders_file, genders_errormsg(gh));

    return gh;
}

/*
 *  Search attr argument for an '=' char indicating an
 *   attr=value pair. If found, nullify '=' and return
 *   pointer to value part.
 *
 *  Returns NULL if no '=' found.
 */
#if !HAVE_GENDERS_QUERY
static char *
_get_val(char *attr)
{
    char *val = NULL;

    if (attr == NULL)
        return (NULL);

    if ((val = strchr(attr, '='))) {
        *val = '\0';
        val++;
    }

    return (val);
}
#endif /* !HAVE_GENDERS_QUERY */

static hostlist_t 
_read_genders_attr(char *query)
{
    hostlist_t hl = NULL;
    char **nodes;
    int len, nnodes;

    if ((len = genders_nodelist_create(gh, &nodes)) < 0)
        errx("%p: genders: nodelist_create: %s\n", genders_errormsg(gh));

#if HAVE_GENDERS_QUERY
    if ((nnodes = genders_query (gh, nodes, len, query)) < 0) {
        errx("%p: Error querying genders for query \"%s\": %s\n", 
                query ? query : "(all)", genders_errormsg(gh));
    }
#else /* !HAVE_GENDERS_QUERY */
    {
        /* query defaults to just an attribute or attribute=value pair */
        char *val;
        val = _get_val(query);
        if ((nnodes = genders_getnodes(gh, nodes, len, query, val)) < 0) {
            errx("%p: Error querying genders for attr \"%s\": %s\n", 
                 query ? query : "(all)", genders_errormsg(gh));
        }
    }
#endif /* HAVE_GENDERS_QUERY */

    hl = _genders_to_hostlist(gh, nodes, nnodes);

    if (genders_nodelist_destroy(gh, nodes) < 0) {
        errx("%p: Error destroying genders node list: %s\n",
                genders_errormsg(gh));
    }

    return hl;
}

static hostlist_t 
_read_genders (List attrs)
{
    hostlist_t   hl = NULL;
    char *    query = NULL;

    if ((attrs == NULL) && (allnodes)) /* Special "all nodes" case */
        return _read_genders_attr (ALL_NODES);

    if ((attrs == NULL) || (list_count (attrs) == 0))
        return NULL;

    while ((query = list_pop (attrs))) {
        hostlist_t l = _read_genders_attr (query);

        if (hl == NULL) {
            hl = l;
        } else {
            hostlist_push_list (hl, l);
            hostlist_destroy (l);
        }
        Free ((void **)&query);
    }

    hostlist_uniq (hl);

    return (hl);
}

static int 
attrval_by_altname (genders_t g, const char *host, const char *attr,
                           char *val, int len)
{   
    char *altname = NULL;
    char *altattr = GENDERS_ALTNAME_ATTRIBUTE;
    int maxlen = _maxnamelen (g);
    int rc = -1;

    altname = Malloc (maxlen + 1);
    memset (altname, 0, maxlen);

    if ((rc = genders_getnodes (g, &altname, 1, altattr, host)) > 0)
        rc = genders_testattr (g, altname, attr, val, sizeof (val));

    Free ((void **) &altname);

    return rc;
}

/*
 *  Parse the value of "pdsh_rcmd_type" and split into user and rcmd
 *   strings, passing rcmd name (if any) in *rp, and user name (if any)
 *   in *up.
 *   
 *  Allows pdsh_rcmd_type to be set to [user@][rcmd], where user@ and 
 *   rcmd are both optional. (i.e. you can set user or rcmd or both)
 */
static int rcmd_type_parse (char *val, char **rp, char **up)
{
    char *p;
    *up = NULL;
    *rp = NULL;

    if ((p = strchr (val, '@'))) {
        *(p)++ = '\0';
        *up = val;
        if (strlen (p) != 0)
            *rp = p;
    } else
        *rp = val;

    return (0);
}

static int
register_genders_rcmd_types (opt_t *opt)
{
    char *host;
    char *rcmd;
    char *user;
    char val[64];
    char rcmd_attr[] = "pdsh_rcmd_type";
    hostlist_iterator_t i = NULL;

    if (!opt->wcoll) 
        return (0);

    /* 
     *  Assume no nodes have "pdsh_rcmd_type" attr if index fails:
     */
    if (genders_index_attrvals (gh, rcmd_attr) < 0)
        return (0);

    i = hostlist_iterator_create (opt->wcoll);
    while ((host = hostlist_next (i))) {
        int rc;
        memset (val, 0, sizeof (val));
        rc = genders_testattr (gh, host, rcmd_attr, val, sizeof (val));

        /*
         *  If host wasn't found, try to see if "host" is the altname
         *   for this node, then lookup with the real name
         */
        if (rc < 0 && (genders_errnum(gh) == GENDERS_ERR_NOTFOUND)) 
            rc = attrval_by_altname (gh, host, rcmd_attr, val, sizeof (val));
        
        rcmd_type_parse (val, &rcmd, &user);

        if (rc > 0) 
            rcmd_register_defaults (host, rcmd, user);

        free (host);
    }

    hostlist_iterator_destroy (i);
            
    return 0;
}

static int 
_delete_all (hostlist_t hl, hostlist_t dl)
{
    int                 rc   = 0;
    char *              host = NULL;
    hostlist_iterator_t i    = hostlist_iterator_create (dl);

    while ((host = hostlist_next (i))) {
        rc += hostlist_delete_host (hl, host);
        free (host);
    }
    hostlist_iterator_destroy (i);
    return (rc);
}

/*
 * vi: tabstop=4 shiftwidth=4 expandtab
 */

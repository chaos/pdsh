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
#  include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/common/hostlist.h"
#include "src/common/err.h"
#include "src/common/xmalloc.h"
#include "src/common/xstring.h"
#include "src/pdsh/xpopen.h"
#include "src/pdsh/mod.h"

#define ALL_NODES "all"

#ifndef LINEBUFSIZE
#  define LINEBUFSIZE 2048
#endif

#if STATIC_MODULES
#  define pdsh_module_info nodeattr_module_info
#  define pdsh_module_priority nodeattr_module_priority
#endif    

int pdsh_module_priority = 50;

static hostlist_t nodeattr_wcoll(opt_t *pdsh_opts);
static int nodeattr_process_opt(opt_t *, int, char *);
static int nodeattr_postop (opt_t *opt);

/*
 *  nodeattr module options
 *   -a      select all nodes except those with pdsh_all_skip attr
 *   -A      select all nodes.
 *   -i      select alternate hostnames from genders
 *   -g attr select all nodes with genders attribute "attr"
 *   -X attr deselect all nodes with genders attribute "attr"
 *
 */
#if !GENDERS_G_ONLY
static bool allnodes   = false;
static bool altnames   = false;
#endif /* !GENDERS_G_ONLY */
static List attrlist = NULL;
static List excllist = NULL;

/* 
 * Export pdsh module operations structure
 */
struct pdsh_module_operations nodeattr_module_ops = {
    (ModInitF)       NULL, 
    (ModExitF)       NULL, 
    (ModReadWcollF)  nodeattr_wcoll, 
    (ModPostOpF)     nodeattr_postop
};

/* 
 * Export module options
 */
struct pdsh_module_option nodeattr_module_options[] = 
 { { 'g', "attribute", "target nodes with specified genders attribute",
     DSH | PCP, (optFunc) nodeattr_process_opt 
   },
   { 'X', "attribute", "exclude nodes with specified genders attribute",
     DSH | PCP, (optFunc) nodeattr_process_opt 
   },
#if !GENDERS_G_ONLY
   { 'a', NULL,        "target all nodes except those with \"pdsh_all_skip\" attr", 
     DSH | PCP, (optFunc) nodeattr_process_opt 
   },
   { 'A', NULL,        "target all nodes",
     DSH | PCP, (optFunc) nodeattr_process_opt
   },
   { 'i', NULL,        "request canonical hostnames if applicable",
     DSH | PCP, (optFunc) nodeattr_process_opt
   },
#endif /* !GENDERS_G_ONLY */
   PDSH_OPT_TABLE_END
 };

/* 
 * Nodeattr module info 
 */
struct pdsh_module pdsh_module_info = {
  "misc",
#if GENDERS_G_ONLY
  "nodeattr-g",
#else
  "nodeattr",
#endif /* GENDERS_G_ONLY */
  "Jim Garlick <garlick@llnl.gov>",
#if GENDERS_G_ONLY
  "target nodes using \"nodeattr -g attr\"",
#else
  "target nodes using \"nodeattr -[ai] [-g attr]\"",
#endif /* GENDERS_G_ONLY */
  DSH | PCP, 

  &nodeattr_module_ops,
  NULL,
  &nodeattr_module_options[0],
};


/*
 *  Static Prototypes:
 */
static hostlist_t _read_genders(List, int iopt);
static int _delete_all (hostlist_t hl, hostlist_t dl);
static List _attrlist_append (List l, char *str);


static int
nodeattr_process_opt(opt_t *pdsh_opts, int opt, char *arg)
{
    switch (opt) {
#if !GENDERS_G_ONLY
    case 'a': 
        excllist = _attrlist_append (excllist, "pdsh_all_skip");
    case 'A':
        allnodes = true;
        break;
    case 'i':
        altnames = true;
        break;
#endif /* !GENDERS_G_ONLY */
    case 'g':
		attrlist = _attrlist_append (attrlist, arg);
        break;
    case 'X':
		excllist = _attrlist_append (excllist, arg);
        break;
    default:
        err("%p: genders_process_opt: invalid option `%c'\n", opt);
        return -1;
        break;
    }
    return 0;
}


/* 
 * Verify options passed to this module
 */
static void
_nodeattr_opt_verify(opt_t *opt)
{
#if !GENDERS_G_ONLY
    if (altnames && !allnodes && (attrlist == NULL)) {
        err("%p: Warning: Ignoring -i without -a or -g\n");
        altnames = false;
    }

    if (allnodes && (attrlist != NULL))
        errx("%p: Do not specify -a with -g\n");
#endif /* !GENDERS_G_ONLY */

    if(opt->wcoll) {
#if !GENDERS_G_ONLY
        if (allnodes)
            errx("%p: Do not specify -a with other node selection options\n");
#endif /* !GENDERS_G_ONLY */
        if (attrlist)
            errx("%p: Do not specify -g with other node selection options\n");
    }
}


static hostlist_t 
nodeattr_wcoll(opt_t *opt)
{
    _nodeattr_opt_verify(opt);

#if GENDERS_G_ONLY
    if (!attrlist)
        return NULL;

    /* GENDERS_G_ONLY, altnames not an issue, always
     * use genders node names.
     */
    return _read_genders(attrlist, true);
#else
    if (!allnodes && !altnames && !attrlist)
        return NULL;

    return _read_genders(attrlist, altnames);
#endif /* !GENDERS_G_ONLY */
}

static int
nodeattr_postop (opt_t *opt)
{
    hostlist_t hl = NULL;
    int iopt = 0;

    if (!opt->wcoll)
        return (0);

    if (!excllist)
        return (0);

    /*
     *  Delete genders *and* altnames from wcoll
     */
    while ((iopt == 0) || (iopt == 1)) {
        if ((hl = _read_genders (excllist, iopt++))) {
            _delete_all (opt->wcoll, hl);
            hostlist_destroy (hl);
        }
    }

    return (0);
}


static hostlist_t 
_read_genders_attr(char *attr, int iopt)
{
    FILE *f;
    hostlist_t hl = hostlist_create(NULL);
    char cmd[LINEBUFSIZE];
    char buf[LINEBUFSIZE];

    /* 
     *   xpopen sets uid back to real user id, so it is ok to use 
     *     "nodeattr" from user's path here
     */
    snprintf(cmd, sizeof(cmd), "%s -%sn %s", _PATH_NODEATTR, 
             iopt ? "" : "r", attr);

    f = xpopen(cmd, "r");
    if (f == NULL)
        errx("%p: error running \"%s\"\n", _PATH_NODEATTR);
    while (fgets(buf, LINEBUFSIZE, f) != NULL) {
        xstrcln(buf, NULL);
        if (hostlist_push(hl, buf) <= 0)
            err("%p: warning: target `%s' not parsed\n", buf);
    }
    if (xpclose(f) != 0)
        errx("%p: error running nodeattr\n");

    return hl;
}

static hostlist_t 
_read_genders (List attrs, int iopt)
{
    ListIterator i  = NULL;
    hostlist_t   hl = NULL;
    char *     attr = NULL;

    if ((attrs == NULL)) /* Special "all nodes" case */
        return _read_genders_attr (ALL_NODES, iopt);

    if ((attrs == NULL) || (list_count (attrs) == 0))
        return NULL;

   if ((i = list_iterator_create (attrs)) == NULL)
        errx ("genders: unable to create list iterator: %m\n");

    while ((attr = list_next (i))) {
        hostlist_t l = _read_genders_attr (attr, iopt);

        if (hl == NULL) {
            hl = l;
		} else {
            hostlist_push_list (hl, l);
            hostlist_destroy (l);
        }
    }

    list_iterator_destroy (i);

    hostlist_uniq (hl);

    return (hl);
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

static void
_free_attr (void *attr)
{
    Free (&attr);
}

/* 
 * Helper function for list_split(). Extract tokens from str.  
 * Return a pointer to the next token; at the same time, advance 
 * *str to point to the next separator.  
 *   sep (IN)   string containing list of separator characters
 *   str (IN)   double-pointer to string containing tokens and separators
 *   RETURN next token
 */
static char *_next_tok(char *sep, char **str)
{
    char *tok;

    /* push str past any leading separators */
    while (**str != '\0' && strchr(sep, **str) != '\0')
        (*str)++;

    if (**str == '\0')
        return NULL;

    /* assign token pointer */
    tok = *str;

    /* push str past token and leave pointing to first separator */
    while (**str != '\0' && strchr(sep, **str) == '\0')
        (*str)++;

    /* nullify consecutive separators and push str beyond them */
    while (**str != '\0' && strchr(sep, **str) != '\0')
        *(*str)++ = '\0';

    return tok;
}

/*
 * Given a list of separators and a string, generate a list
 *   sep (IN)   string containing separater characters
 *   str (IN)   string containing tokens and separators
 *   RETURN     new list containing all tokens
 */
static List _list_split(char *sep, char *str)
{
    List new = list_create((ListDelF) _free_attr);
    char *tok;

    if (sep == NULL)
        sep = " \t";

    while ((tok = _next_tok(sep, &str)) != NULL) {
        if (strlen(tok) > 0)
            list_append(new, Strdup(tok));
    }

    return new;
}

/*
 *  Split comma-separated "attrs" from str and append to list `lp'
 */
static List _attrlist_append (List l, char *str)
{
    List tmp = _list_split (",", str);
    ListIterator i = NULL;
    char *attr = NULL;

    if (l == NULL) 
        return ((l = tmp));

    i = list_iterator_create (tmp);
    while ((attr = list_next (i))) 
        list_append (l, Strdup(attr));
    list_destroy (tmp);

    return (l);
}




/*
 * vi: tabstop=4 shiftwidth=4 expandtab
 */

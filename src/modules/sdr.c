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

#include <assert.h>
#include <string.h>

#include "src/pdsh/wcoll.h"
#include "src/pdsh/mod.h"
#include "src/pdsh/xpopen.h"
#include "src/common/xmalloc.h"
#include "src/common/err.h"
#include "src/common/xstring.h"

#define SPACES "\t\n "

#define LINEBUFSIZE 2048
/* some handy SP constants */
/* NOTE: degenerate case of one node per frame, nodes would be 1, 17, 33,... */
#define MAX_SP_NODES        512
#define MAX_SP_NODES_PER_FRAME  16
#define MAX_SP_NODE_NUMBER (MAX_SP_NODES * MAX_SP_NODES_PER_FRAME - 1)

#if STATIC_MODULES
#  define pdsh_module_info sdr_module_info
#  define pdsh_module_priority sdr_module_priority
#endif    

int pdsh_module_priority = DEFAULT_MODULE_PRIORITY;

static hostlist_t read_sdr(opt_t *opt);

static int sdr_process_opt(opt_t *, int, char *);

static bool allnodes = false;
static bool altnames = false;
static bool verify   = false;
static bool global   = false;

/*
 *  Export generic module functions
 */
struct pdsh_module_operations sdr_module_ops = {
    (ModInitF)       NULL, 
    (ModExitF)       NULL, 
    (ModReadWcollF)  read_sdr,
    (ModPostOpF)     NULL,
};

/* 
 * Export rcmd module operations
 */
struct pdsh_rcmd_operations sdr_rcmd_ops = {
    (RcmdInitF)  NULL,
    (RcmdSigF)   NULL,
    (RcmdF)      NULL,
};

/* 
 * Export module options
 */
struct pdsh_module_option sdr_module_options[] = 
 { { 'a', NULL, "target all nodes", 
     DSH | PCP, (optFunc) sdr_process_opt
   },
   { 'v', NULL, "with -a, verify nodes are up using host/switch_responds",
     DSH | PCP, (optFunc) sdr_process_opt
   },
   { 'i', NULL, "use alternate hostnames from SDR",
     DSH | PCP, (optFunc) sdr_process_opt
   },
   { 'G', NULL, "with -a, run on all SP partitions",
     DSH | PCP, (optFunc) sdr_process_opt
   },
   PDSH_OPT_TABLE_END
 };

/* 
 * Sdr module info 
 */
struct pdsh_module pdsh_module_info = {
  "misc",
  "sdr",
  "Jim Garlick <garlick@llnl.gov>",
  "Support for SDR on IBM SP",
  DSH | PCP, 

  &sdr_module_ops,
  &sdr_rcmd_ops,
  &sdr_module_options[0],
};

static int sdr_process_opt(opt_t *pdsh_opt, int opt, char *arg)
{
    switch (opt) {
     case 'a': 
        allnodes = true; 
        break;
     case 'i': 
        altnames = true; 
        break;
     case 'v': 
        verify = true;   
        break;
     case 'G': 
        global = true;
        break;
     default:  
        errx("%p: badness factor high in sdr module\n");
        break;
    }
    return 0;
}

static List _list_split(char *sep, char *str);
static char *_list_nth(List l, int n);
static void _free_name(void *name);
static char *_next_tok(char *sep, char **str);
static void _sdr_getnames(bool Gopt, char *nameType, char *nodes[]);
static void _sdr_getresp(bool Gopt, char *nameType, bool resp[]);
static void _sdr_getswitchname(char *switchName, int len);
static int _sdr_numswitchplanes(void);
static hostlist_t sdr_wcoll(bool Gopt, bool iopt, bool vopt);

static hostlist_t read_sdr(opt_t *opt)
{
    if (allnodes && opt->wcoll)
        errx("%p: Do not specify -a with other node selection options\n");

    if (altnames && !allnodes)
        err("%p: Warning: Ignoring -i without -a\n");

    if (verify && !allnodes)
        err("%p: Warning: Ignoring -v without -a\n");

    if (!allnodes)
        return NULL;

    return sdr_wcoll(global, altnames, verify);
}


static int _sdr_numswitchplanes(void)
{
    FILE *f;
    List words;
    char cmd[LINEBUFSIZE];
    char buf[LINEBUFSIZE];
    int n;

    snprintf(cmd, sizeof(cmd), "%s -x SP number_switch_planes",
             _PATH_SDRGETOBJECTS);

    f = xpopen(cmd, "r");

    if (f == NULL)
        errx("%p: error running %s\n", _PATH_SDRGETOBJECTS);
    while (fgets(buf, LINEBUFSIZE, f) != NULL) {
        words = _list_split(NULL, buf);
        assert(list_count(words) == 1);
        n = atoi(_list_nth(words, 0));
        list_destroy(words);
    }
    if (xpclose(f) != 0)
        err("%p: nonzero return code from %s\n", _PATH_SDRGETOBJECTS);

    return n;
}

static void _sdr_getswitchname(char *switchName, int len)
{
    FILE *f;
    List words;
    char cmd[LINEBUFSIZE];
    char buf[LINEBUFSIZE];

    snprintf(cmd, sizeof(cmd), "%s -x Switch switch_number==1 switch_name",
             _PATH_SDRGETOBJECTS);
    f = xpopen(cmd, "r");
    if (f == NULL)
        errx("%p: error running %s\n", _PATH_SDRGETOBJECTS);
    while (fgets(buf, LINEBUFSIZE, f) != NULL) {
        words = _list_split(NULL, buf);
        assert(list_count(words) == 1);
        snprintf(switchName, len, _list_nth(words, 0));
        list_destroy(words);
    }
    xpclose(f);
}

/*
 * Query the SDR for switch_responds or host_responds for all nodes and return
 * the results in an array indexed by node number.
 *      Gopt (IN)       pass -G to SDRGetObjects
 *      nameType (IN)   either "switch_responds" or "host_responds"
 *      resp (OUT)      array of boolean, indexed by node number
 */
static void _sdr_getresp(bool Gopt, char *nameType, bool resp[])
{
    int nn, switchplanes;
    FILE *f;
    List words;
    char cmd[LINEBUFSIZE];
    char buf[LINEBUFSIZE];
    char *attr = "host_responds";

    switchplanes = 1;

    /* deal with Colony switch attribute name change */
    if (!strcmp(nameType, "switch_responds")) {
        _sdr_getswitchname(buf, sizeof(buf));
        if (!strcmp(buf, "SP_Switch2")) {
            switchplanes = _sdr_numswitchplanes();
            attr = (switchplanes == 1) ? "switch_responds0" :
                "switch_responds0 switch_responds1";
        } else
            attr = "switch_responds";
    }

    snprintf(cmd, sizeof(cmd), "%s %s -x %s node_number %s",
             _PATH_SDRGETOBJECTS, Gopt ? "-G" : "", nameType, attr);
    f = xpopen(cmd, "r");
    if (f == NULL)
        errx("%p: error running %s\n", _PATH_SDRGETOBJECTS);
    while (fgets(buf, LINEBUFSIZE, f) != NULL) {
        words = _list_split(NULL, buf);
        assert(list_count(words) == (1 + switchplanes));
        nn = atoi(_list_nth(words, 0));
        assert(nn >= 0 && nn <= MAX_SP_NODE_NUMBER);
        if (switchplanes == 1)
            resp[nn] = (atoi(_list_nth(words, 1)) == 1);
        else if (switchplanes == 2)
            resp[nn] = (atoi(_list_nth(words, 1)) == 1 ||
                        atoi(_list_nth(words, 1)) == 1);
        else
            errx("%p: number_switch_planes > 2 not supported\n");
        list_destroy(words);
    }
    xpclose(f);
}

/*
 * Query the SDR for hostnames of all nodes and return the results in an 
 * array indexed by node number.
 *      Gopt (IN)       pass -G to SDRGetObjects
 *      nameType (IN)   either "initial_hostname" or "reliable_hostname"
 *      resp (OUT)      array of hostnames indexed by node number (heap cpy)
 */
static void _sdr_getnames(bool Gopt, char *nameType, char *nodes[])
{
    int nn;
    FILE *f;
    List words;
    char cmd[LINEBUFSIZE];
    char buf[LINEBUFSIZE];

    snprintf(cmd, sizeof(cmd), "%s %s -x Node node_number %s",
             _PATH_SDRGETOBJECTS, Gopt ? "-G" : "", nameType);
    f = xpopen(cmd, "r");
    if (f == NULL)
        errx("%p: error running %s\n", _PATH_SDRGETOBJECTS);
    while (fgets(buf, LINEBUFSIZE, f) != NULL) {
        words = _list_split(NULL, buf);
        assert(list_count(words) == 2);

        nn = atoi(_list_nth(words, 0));
        assert(nn >= 0 && nn <= MAX_SP_NODE_NUMBER);
        nodes[nn] =  Strdup(_list_nth(words, 1));
        list_destroy(words);
    }
    xpclose(f);
}


/*
 * Get the wcoll from the SDR.  
 *      Gopt (IN)       pass -G to SDRGetObjects
 *      altnames (IN)   ask for initial_hostname instead of reliable_hostname
 *      vopt (IN)       verify switch_responds/host_responds
 *      RETURN          new list containing hostnames
 */
static hostlist_t sdr_wcoll(bool Gopt, bool iopt, bool vopt)
{
    hostlist_t new;
    char *inodes[MAX_SP_NODE_NUMBER + 1], *rnodes[MAX_SP_NODE_NUMBER + 1];
    bool sresp[MAX_SP_NODE_NUMBER + 1], hresp[MAX_SP_NODE_NUMBER + 1];
    int nn;

    /*
     * Build arrays of hostnames indexed by node number.  Array is size 
     * MAX_SP_NODE_NUMBER, with NULL pointers set for unused nodes.
     */
    for (nn = 0; nn <= MAX_SP_NODE_NUMBER; nn++) {
        inodes[nn] = NULL;
        rnodes[nn] = NULL;
    }
    if (iopt)
        _sdr_getnames(Gopt, "initial_hostname", inodes);
    else
        _sdr_getnames(Gopt, "reliable_hostname", rnodes);

    /*
     * Gather data needed to process -v.
     */
    if (vopt) {
        if (iopt)
            _sdr_getresp(Gopt, "switch_responds", sresp);
        _sdr_getresp(Gopt, "host_responds", hresp);
    }

    /*
     * Collect and return the nodes.  If -v was specified and a node is 
     * not responding, substitute the alternate name; if that is not 
     * responding, skip the node.
     */
    new = hostlist_create("");
    for (nn = 0; nn <= MAX_SP_NODE_NUMBER; nn++) {
        if (inodes[nn] != NULL || rnodes[nn] != NULL) {
            if (vopt) {         /* initial_host */
                if (iopt && sresp[nn] && hresp[nn])
                    hostlist_push(new, inodes[nn]);
                else if (!iopt && hresp[nn])    /* reliable_host */
                    hostlist_push(new, rnodes[nn]);
            } else {
                if (iopt)       /* initial_host */
                    hostlist_push(new, inodes[nn]);
                else            /* reliable_host */
                    hostlist_push(new, rnodes[nn]);
            }
            if (inodes[nn] != NULL)     /* free heap cpys */
                Free((void **) &inodes[nn]);
            if (rnodes[nn] != NULL)
                Free((void **) &rnodes[nn]);
        }
    }

    return new;
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
    List new = list_create((ListDelF) _free_name);
    char *tok;

    if (sep == NULL)
        sep = SPACES;

    while ((tok = _next_tok(sep, &str)) != NULL) {
        if (strlen(tok) > 0)
            list_append(new, Strdup(tok));
    }

    return new;
}


static char *_list_nth(List l, int n)
{
    int i = 0;
    char *name = NULL;
    ListIterator itr = list_iterator_create(l);

    while ((name = list_next(itr))) {
        if (i++ == n) break;
    }
    list_iterator_destroy(itr);
    return name;
}

static void _free_name(void *name)
{
    Free(&name);
}


/*
 * vi: tabstop=4 shiftwidth=4 expandtab
 */

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

static int sdr_init (void);
static int sdr_exit (void);
static hostlist_t read_sdr (opt_t *opt);
static int sdr_postop (opt_t *);

static int sdr_process_opt(opt_t *, int, char *);

/*
 *  Export generic module functions
 */
struct pdsh_module_operations sdr_module_ops = {
    (ModInitF)       sdr_init, 
    (ModExitF)       sdr_exit, 
    (ModReadWcollF)  read_sdr,
    (ModPostOpF)     sdr_postop,
};

/* 
 * Export module options
 */
struct pdsh_module_option sdr_module_options[] = 
 { { 'a', NULL, "target all nodes", 
     DSH | PCP, (optFunc) sdr_process_opt
   },
   { 'v', NULL, "verify nodes are up using host/switch_responds",
     DSH | PCP, (optFunc) sdr_process_opt
   },
   { 'i', NULL, "translate to alternate/initial hostnames from SDR (if applicable)",
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
  NULL,
  &sdr_module_options[0],
};



/*
 *  Data cache for SDR information.
 *  XXX: Hash by node number instead of leaving room for
 *       all possible nodes
 */
struct sdr_info {
    char *hostname;
    char *reliable_hostname;
    bool switch_responds;
    bool host_responds;
};

static bool sdr_initialized = false;
static struct sdr_info * sdrcache[MAX_SP_NODE_NUMBER];

/* 
 *  Global options
 */
static bool allnodes = false;
static bool altnames = false;
static bool verify   = false;
static bool global   = false;


/*
 *  Required static forward declarations
 */
static struct sdr_info * sdr_info_create (char *host, char *rhost);
static void sdr_info_destroy (struct sdr_info *s);

static hostlist_t _sdr_filter (hostlist_t hl, bool iopt, bool verify);
static hostlist_t _sdr_wcoll(bool Gopt);
static struct sdr_info * _find_node (const char *name, int *rhost);
static hostlist_t _sdr_reliable_names (void);
static void _sdr_getnames(bool Gopt);
static void _sdr_getresp (bool Gopt);

static List _list_split(char *sep, char *str);
static char *_list_nth(List l, int n);
static void _free_name(void *name);
static char *_next_tok(char *sep, char **str);


/*
 * module interface functions
 */

static int sdr_init (void)
{
	int i;
	for (i = 0; i < MAX_SP_NODE_NUMBER; i++)
		sdrcache[i] = NULL;
	return (0);
}

static int sdr_exit (void)
{
    int i;
    for (i = 0; i < MAX_SP_NODE_NUMBER; i++)
        sdr_info_destroy (sdrcache[i]);
    return (0);
}

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


static hostlist_t read_sdr(opt_t *opt)
{
	if (!allnodes)
		return (NULL); 

    if (allnodes && opt->wcoll)
        errx("%p: Do not specify -a with other node selection options\n");

    return _sdr_wcoll (global);
}

static int sdr_postop (opt_t *opt)
{
    hostlist_t hl; 

    if (!verify && !altnames)
        return (0);

    if (!opt->wcoll || (hostlist_count (opt->wcoll) == 0))
        return (0);

    if (!sdr_initialized)
        _sdr_getnames (global);

    if (verify) 
        _sdr_getresp (global);

    hl = _sdr_filter (opt->wcoll, altnames, verify);
    hostlist_destroy (opt->wcoll);
    opt->wcoll = hl;

    return (0);
}

/*
 * Other functions
 */


/*
 * Get the wcoll from the SDR.  
 *      Gopt (IN)       pass -G to SDRGetObjects
 *      RETURN          new list containing hostnames (reliable by default)
 */
static hostlist_t _sdr_wcoll (bool Gopt)
{
	/*
	 *  Cache SDR reliable and initial hostnames
	 */
	_sdr_getnames (Gopt);

    return _sdr_reliable_names ();
}


/*
 * Filter hostlist `hl' using SDR attributes.
 *     iopt     convert reliable hostnames to initial and vice versa.
 *     verify   remove hosts that are not responding on the corresponding
 *              interface (i.e. switch for initial hostnames, eth otherwise)
 *     RETURN   new list containing filtered hosts.
 */
static hostlist_t _sdr_filter (hostlist_t hl, bool iopt, bool verify)
{
    char *host = NULL;
    hostlist_t new  = hostlist_create (NULL);
    hostlist_iterator_t i = hostlist_iterator_create (hl);
    struct sdr_info *s = NULL;

    while ((host = hostlist_next (i))) {
        int r = 0;

        if ((s = _find_node (host, &r)) == NULL) {
            hostlist_push_host (new, host);
            continue;
        }

        if (iopt) 
            r = !r;

        if (!verify || (r ? s->host_responds : s->switch_responds))
           hostlist_push_host (new, r ? s->reliable_hostname : s->hostname);

        free (host);
    }

    hostlist_iterator_destroy (i);

    return (new);
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

static char * _sdr_switch_attr (int *numswitchplanes)
{
    FILE *f;
    List words;
    char cmd[LINEBUFSIZE];
    char buf[LINEBUFSIZE];
    int n;

	static char * attr[] = {
		"switch_responds",
		"switch_responds0",
		"switch_responds0 switch_responds1"
	};

	_sdr_getswitchname(buf, sizeof(buf));
	if (strcmp(buf, "SP_Switch2") != 0) {
		*numswitchplanes = 1;
		return (attr[0]);
	}
 
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

	*numswitchplanes = n;
	return (attr[n]);
}

static void _sdr_cache_hresp_line (char *buf)
{
	List words = NULL;
	int  nn    = -1;

	words = _list_split (NULL, buf);
	assert(list_count (words) == 2);

	nn = atoi (_list_nth (words, 0));
	assert (nn >= 0 && nn <= MAX_SP_NODE_NUMBER);
    /* 
     * Ignore host_responds for hosts without node information
     */
    if (sdrcache[nn] != NULL)
        sdrcache[nn]->host_responds = (atoi (_list_nth (words, 1)) == 1);

	return;
}

static void _sdr_cache_sresp_line (char *buf, int switchplanes)
{
	List words = NULL;
	int  nn    = -1;
	struct sdr_info *s;

	words = _list_split (NULL, buf);
	assert(list_count (words) == (1 + switchplanes));

	nn = atoi (_list_nth (words, 0));
	assert (nn >= 0 && nn <= MAX_SP_NODE_NUMBER);
	assert (sdrcache[nn] != NULL);

	s = sdrcache[nn];

	s->switch_responds = (atoi(_list_nth(words, 1)) == 1);
	if (switchplanes == 2)
		s->switch_responds = s->switch_responds || (atoi(_list_nth (words, 1)));

	return;
}

static void _sdr_cache_name_line (char *buf)
{
	char *name  = NULL;
	char *rname = NULL;
	List words  = NULL;
	int  nn     = -1; 
    char *p;

	words = _list_split (NULL, buf);
	assert (list_count(words) == 3);

	nn = atoi (_list_nth (words, 0));
	assert (nn >= 0 && nn <= MAX_SP_NODE_NUMBER);

	name = _list_nth (words, 1);
	rname = _list_nth (words, 2);

	if ((p = strchr (name, '.')))
		*p = '\0';

	if ((p = strchr (rname, '.')))
		*p = '\0';

	sdrcache[nn] = sdr_info_create (name, rname);

	list_destroy (words);
	return;
}

static void _sdr_getresp (bool Gopt)
{
    FILE *f;
    char cmd[LINEBUFSIZE];
    char buf[LINEBUFSIZE];
	int nswitchplanes;

    snprintf (cmd, sizeof(cmd), 
			 "%s %s -x host_responds node_number host_responds",
             _PATH_SDRGETOBJECTS, Gopt ? "-G" : "");

    if ((f = xpopen (cmd, "r")) == NULL)
        errx("%p: error running %s\n", _PATH_SDRGETOBJECTS);

    while (fgets (buf, LINEBUFSIZE, f) != NULL) 
		_sdr_cache_hresp_line (buf);

    snprintf (cmd, sizeof(cmd), 
			 "%s %s -x switch_responds node_number %s",
             _PATH_SDRGETOBJECTS, Gopt ? "-G" : "", 
			 _sdr_switch_attr (&nswitchplanes));

    if ((f = xpopen (cmd, "r")) == NULL)
        errx("%p: error running %s\n", _PATH_SDRGETOBJECTS);

    while (fgets (buf, LINEBUFSIZE, f) != NULL) 
		_sdr_cache_sresp_line (buf, nswitchplanes);

	xpclose (f);

	return;
}

/*
 * Query the SDR for hostnames of all nodes and return the results in an 
 * array indexed by node number.
 *      Gopt (IN)       pass -G to SDRGetObjects
 */
static void _sdr_getnames(bool Gopt)
{
    FILE *f;
    char cmd[LINEBUFSIZE];
    char buf[LINEBUFSIZE];

    snprintf (cmd, sizeof(cmd), 
			 "%s %s -x Node node_number initial_hostname reliable_hostname",
             _PATH_SDRGETOBJECTS, Gopt ? "-G" : "");

    if ((f = xpopen (cmd, "r")) == NULL)
        errx("%p: error running %s\n", _PATH_SDRGETOBJECTS);

    while (fgets (buf, LINEBUFSIZE, f) != NULL) 
		_sdr_cache_name_line (buf);

    xpclose(f);

    sdr_initialized = true;
}

static hostlist_t _sdr_reliable_names ()
{
	hostlist_t hl = hostlist_create (NULL);
	int i;

	for (i = 0; i < MAX_SP_NODE_NUMBER; i++) {
		if (sdrcache[i] != NULL)
			hostlist_push_host (hl, sdrcache[i]->reliable_hostname);
	}

	return (hl);
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

static struct sdr_info * sdr_info_create (char *host, char *rhost)
{
	struct sdr_info *s = Malloc (sizeof (*s));

	s->hostname = Strdup (host);
	s->reliable_hostname = Strdup (rhost);

	s->host_responds = false;
	s->switch_responds = false;

	return (s);
}

static void sdr_info_destroy (struct sdr_info *s)
{
    if (s == NULL)
        return;

	if (s->hostname)
		Free ((void **) &s->hostname);
	if (s->reliable_hostname)
		Free ((void **) &s->reliable_hostname);

	Free ((void **) &s);

	return;
}

static struct sdr_info * _find_node (const char *name, int *rhost)
{
    int i;

    for (i = 0; i < MAX_SP_NODE_NUMBER; i++) {
        struct sdr_info *s = sdrcache[i];

        if (s == NULL)
            continue;

        if (strncmp (name, s->reliable_hostname, 
                     strlen (s->reliable_hostname)) == 0) {
            if (rhost != NULL)
                *rhost = 1;
            return (s);
        }

        if (strncmp (name, s->hostname, strlen (s->hostname)) == 0) {
            if (rhost != NULL)
                *rhost = 0;
            return (s);
        }
    }

    return (NULL);
}

/*
 * vi: tabstop=4 shiftwidth=4 expandtab
 */

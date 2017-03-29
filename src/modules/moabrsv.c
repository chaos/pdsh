/*****************************************************************************
 *  $Id: moabrsv.c $
 *****************************************************************************/

/*
 *  moabrsv.c created using torque.c as template.
 *  Troy Baer <troy@osc.edu>
 */

/*****************************************************************************
 *  Copyright (C) 2001-2007 The Regents of the University of California.
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



#if HAVE_CONFIG_H
#  include "config.h"
#endif

#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>

#include "src/common/hostlist.h"
#include "src/common/split.h"
#include "src/common/err.h"
#include "src/common/xmalloc.h"
#include "src/common/xstring.h"
#include "src/pdsh/xpopen.h"
#include "src/pdsh/ltdl.h"
#include "src/pdsh/mod.h"
#include "src/pdsh/opt.h"

#if STATIC_MODULES
#  define pdsh_module_info moabrsv_module_info
#  define pdsh_module_priority moabrsv_module_priority
#endif

/*
 * There's not a public C API for talking to Moab, but there is an
 * "XML API" using mrsvctl, mdiag, and friends.
 */
#include <libxml/tree.h>
#include <libxml/parser.h>
#define RSVQUERY "mrsvctl -q --xml %s"
#define RSV_ELEMENT "rsv"
#define NODELIST_PROP "AllocNodeList"


/*
 *  Give this module low priority
 */
int pdsh_module_priority = 10;


/*
 *  Call this module after all option processing.
 *    Calling the module in postop allows us to be sure that all other
 *    modules had a chance to update the wcoll.
 */
static int mod_moabrsv_init(void);
static int mod_moabrsv_wcoll(opt_t *opt);
static int mod_moabrsv_exit(void);
static hostlist_t _moabrsv_wcoll(List rsvids);
static int moabrsv_process_opt(opt_t *, int opt, char *arg);

static List rsv_list = NULL;

/*
 *  Export generic pdsh module options
 */
struct pdsh_module_operations moabrsv_module_ops = {
    (ModInitF)       mod_moabrsv_init,
    (ModExitF)       mod_moabrsv_exit,
    (ModReadWcollF)  mod_moabrsv_wcoll,
    (ModPostOpF)     NULL
};


/*
 * Export module options
 */
struct pdsh_module_option moabrsv_module_options[] =
 {
   { 'j', "rsvid,...",
     "Run on nodes allocated to Moab reservation(s)",
     DSH | PCP, (optFunc) moabrsv_process_opt
   },
   PDSH_OPT_TABLE_END
 };

/*
 * MOABRSV module info
 */
struct pdsh_module pdsh_module_info = {
  "misc",
  "moabrsv",
  "Troy Baer <troy@osc.edu>",
  "Attempt to create wcoll from Moab reservation",
  DSH | PCP,
  &moabrsv_module_ops,
  NULL,
  &moabrsv_module_options[0],
};


static int mod_moabrsv_init(void)
{
    return (0);
}


static int
moabrsv_process_opt(opt_t *pdsh_opts, int opt, char *arg)
{
    switch (opt) {
    case 'j':
        rsv_list = list_split_append(rsv_list, ",", arg);
        break;
    default:
        break;
    }

    return (0);
}

static int
mod_moabrsv_exit(void)
{
    if (rsv_list)
        list_destroy (rsv_list);

    return (0);
}

/*
 *  If no wcoll has been established by this time,  set wcoll to the list
 *  of nodes allocated to that reservation.
 */
static int mod_moabrsv_wcoll(opt_t *opt)
{
    if (rsv_list && opt->wcoll)
        errx("%p: do not specify -j with any other node selection option.\n");

    if (!opt->wcoll)
            opt->wcoll = _moabrsv_wcoll(rsv_list);

    return 0;
}

int _MRSV_processXmlNode(xmlNode *node, char *nodelist)
{
  int retcode = 0;
  xmlNode *thisnode;

  for ( thisnode = node ; thisnode ; thisnode = thisnode->next ) {
      if ( thisnode->type==XML_ELEMENT_NODE &&
	     strncmp((char *)thisnode->name,RSV_ELEMENT,strlen(RSV_ELEMENT))==0 )	{
	if ( xmlHasProp(thisnode,(xmlChar *)NODELIST_PROP) ) {
	      if ( strlen(nodelist)==0 ) {
		  sprintf(nodelist,"%s",xmlGetProp(thisnode,(xmlChar *)NODELIST_PROP));
	      }
	      else {
		  sprintf(nodelist,"%s,%s",nodelist,xmlGetProp(thisnode,(xmlChar *)NODELIST_PROP));
	      }
	  }
      }
      retcode = retcode & _MRSV_processXmlNode(thisnode->children,nodelist);
  }
  return retcode;
}

char *_MRSV_nodes_from_rsv(char *rsvid) {
    if ( rsvid!=NULL ) {
        char *nodelist;
	char *cmd;
	FILE *fp;
	int fd;
	xmlDocPtr rsvinfo;
	xmlNode *root;
	
	xmlInitParser();
	nodelist = (char *)calloc(FILENAME_MAX,sizeof(char));
	cmd = (char *)calloc(FILENAME_MAX,sizeof(char));
	sprintf(cmd,RSVQUERY,rsvid);
	fp = popen(cmd,"r");
	if ( fp==NULL ) {
  	  fprintf(stderr,"No reservation data for %s\n",rsvid);
	  return NULL;
      }
      else {
	  fd = fileno(fp);
	  rsvinfo = xmlReadFd(fd,NULL,NULL,0);
	  if ( rsvinfo!=NULL ) {
	      root = xmlDocGetRootElement(rsvinfo);
	      _MRSV_processXmlNode(root,nodelist);
	      xmlFreeDoc(rsvinfo);
	  }
	  else {
	      fprintf(stderr,"No reservation data for %s\n",rsvid);
	      free(nodelist);
	      nodelist = NULL;
	  }
	  pclose(fp);
      }
      free(cmd);
      xmlCleanupParser();
      return nodelist;
  }
  else {
      return NULL;
  }
}


static hostlist_t _hl_append_node(hostlist_t hl, char *node)
{
    if (hl == NULL)
        return (hostlist_create(node));
    else
        hostlist_push_host(hl,node);
    return (hl);
}

static hostlist_t _add_rsvnodes(hostlist_t hl, char *rsvid) {
    char *rsvnodes;
    List node_list;
    ListIterator li;
    char *node;

    rsvnodes = _MRSV_nodes_from_rsv(rsvid);
    if ( rsvnodes!=NULL ) {
        node_list = list_split(",",rsvnodes);
	li = list_iterator_create(node_list);
	list_destroy(node_list);
	while ( (node=list_next(li)) ) {
	    _hl_append_node(hl,node);
	}
    }

    return hl;
}

static hostlist_t _moabrsv_wcoll(List rsvlist)
{
    hostlist_t hl   = NULL;
    ListIterator li = NULL;
    char *rsvid;

    if ( rsvlist==NULL ) {
        return (NULL);
    }
    else {
        li = list_iterator_create(rsvlist);
	while ( (rsvid=list_next(li)) ) {
            hl = _add_rsvnodes(hl,rsvid);
	}
    }

    if (hl)
        hostlist_uniq (hl);
    return (hl);
}

/*
 * vi: tabstop=4 shiftwidth=4 expandtab
 */

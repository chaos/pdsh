/*****************************************************************************
 *  $Id: torque.c $
 *****************************************************************************/

/*
 *  torque.c created using slurm.c as template.
 *  Mattias Slabanja <don.fanucci@gmail.com>
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

#include <pbs_ifl.h>
#include <pbs_error.h>

#if STATIC_MODULES
#  define pdsh_module_info torque_module_info
#  define pdsh_module_priority torque_module_priority
#endif
/*
 *  Give this module low priority
 */
int pdsh_module_priority = 10;


/*
 *  Call this module after all option processing. The module will only
 *    try to read the PBS_JOBID if opt->wcoll is not already set.
 *    Calling the module in postop allows us to be sure that all other
 *    modules had a chance to update the wcoll.
 */
static int mod_torque_init(void);
static int mod_torque_wcoll(opt_t *opt);
static int mod_torque_exit(void);
static hostlist_t _torque_wcoll(List jobids);
static int torque_process_opt(opt_t *, int opt, char *arg);

static List job_list = NULL;

/*
 *  Export generic pdsh module options
 */
struct pdsh_module_operations torque_module_ops = {
    (ModInitF)       mod_torque_init,
    (ModExitF)       mod_torque_exit,
    (ModReadWcollF)  mod_torque_wcoll,
    (ModPostOpF)     NULL
};


/*
 * Export module options
 */
struct pdsh_module_option torque_module_options[] =
 {
   { 'j', "jobid,...",
     "Run on nodes allocated to TORQUE job(s)",
     DSH | PCP, (optFunc) torque_process_opt
   },
   PDSH_OPT_TABLE_END
 };

/*
 * TORQUE module info
 */
struct pdsh_module pdsh_module_info = {
  "misc",
  "torque",
  "Mattias Slabanja <don.fanucci@gmail.com>",
  "Attempt to create wcoll from PBS_JOBID env var",
  DSH | PCP,
  &torque_module_ops,
  NULL,
  &torque_module_options[0],
};


static int mod_torque_init (void)
{
    return (0);
}


static int32_t str2jobid (const char *str)
{
    char *p = NULL;
    long int jid;

    if (str == NULL)
        return (-1);

    jid = strtoul (str, &p, 10);

    if( *p != '\0' )
        errx ("%p: invalid jobid format \"%s\" for -j\n", str);

    return ((int32_t) jid);
}


static int
torque_process_opt(opt_t *pdsh_opts, int opt, char *arg)
{
    switch (opt) {
    case 'j':
        job_list = list_split_append (job_list, ",", arg);
        break;
    default:
        break;
    }

    return (0);
}

static int
mod_torque_exit(void)
{
    if (job_list)
        list_destroy (job_list);

    return (0);
}

/*
 *  If no wcoll has been established by this time, look for the
 *    PBS_JOBID env var, and set wcoll to the list of nodes allocated
 *    to that job.
 */
static int mod_torque_wcoll(opt_t *opt)
{
    if (job_list && opt->wcoll)
        errx("%p: do not specify -j with any other node selection option.\n");

    if (!opt->wcoll)
            opt->wcoll = _torque_wcoll (job_list);

    return 0;
}

static void _create_fq_jobid(char *dst, const char *jobid, const char *servername){
  /*
   *  Create fully qualified jobid, "<integer>.<servername>"
   */
    if ( str2jobid(jobid) < 0 ){
        *dst = '\0';
        return;
    }
    strncpy(dst, jobid, PBS_MAXSEQNUM);
    strncat(dst, ".", 1);
    strncat(dst, servername, PBS_MAXSERVERNAME);
}

static char *_next_hname(char *p){
    /*
     *   Find next hostname in a "exechost list".
     *   Find next '/'-character, then find next '+'-character,
     */

    if( p == NULL ) return NULL;

    while( *p != '/' )
        if( *p++ == '\0' ) return NULL;

    while( *p != '+' )
        if( *p++ == '\0' ) return NULL;

    /* And finally step past the '+'-character */
    if( *p++ == '\0' ) return NULL;

    return p;
}

static void _copy_hname(char *dst, const char *src){
    /* Copy at most PBS_MAXHOSTNAME-1 characters.
       Hostname in src ends before a '/'-character.
     */
    const char *q = src;

    while( (*src != '\0') && \
           (*src != '/') && \
           (src - q) < (PBS_MAXHOSTNAME-2) ){
        *dst++ = *src++;
    }
    *dst = '\0';
}

static hostlist_t _hl_append_host (hostlist_t hl, char *node)
{
    if (hl == NULL)
        return (hostlist_create (node));
    else
        hostlist_push_host (hl, node);
    return (hl);
}

static hostlist_t _add_jobnodes(hostlist_t hl, int connect, char *jobdesc){
    /*
     *  Add the nodes allocated by job jobdesc to hostlist hl.
     */
    struct batch_status *status;
    struct attrl a_exechost = {NULL, ATTR_exechost, NULL, NULL};
    char *p, hname[PBS_MAXHOSTNAME];

    if( (status = pbs_statjob(connect, jobdesc, &a_exechost, EXECQUEONLY)) == NULL ){
        err ("%p: Failed to retrieve information for job %s: (%d) %s\n",
                jobdesc, pbs_errno, pbs_strerror(pbs_errno));
    }
    else if ( status->attribs == NULL ){
        /* We end up here e.g. if the job is not started.
	 */
        //errx("Job %s has no allocated nodes.\n", jobdesc);
        ;
    }
    else {
        /* An "exechost"-list is available for the job.
         * exechosts are on the format
         * host/slot+host/slot+host/slot+..., i.e. hostname slash integer plus ...
         * Just ignore the slash-integer parts, split at the '+'-signs,
         * and let hostlist_uniq deal with duplicates later on.
         */

        for(p = status->attribs->value; p != NULL; p = _next_hname(p)){
            _copy_hname(hname, p);
            hl = _hl_append_host(hl, hname);
        }
        pbs_statfree(status);
    }
    return(hl);
}

static hostlist_t _torque_wcoll (List joblist)
{
    hostlist_t hl   = NULL;
    ListIterator li = NULL;
    char jobid[PBS_MAXCLTJOBID];
    char *envjobid  = NULL;
    char *p;

    struct batch_status *status;
    struct attrl a_servername = {NULL, ATTR_servername, NULL, NULL};
    char servername[PBS_MAXSERVERNAME];
    int connect;

    /*
     *   A joblist provided via the command line will take presence over PBS_JOBID.
     *   If there is a joblist, don't bother checking PBS_JOBID.
     */
    if ((joblist == NULL) && ((envjobid = getenv("PBS_JOBID")) == NULL))
        return (NULL);


    /* Connect to "default_server". */
    if( (connect = pbs_connect(NULL)) < 0 ){
        errx("%p: Failed to connect to torque server %s: (%d) %s\n",
             pbs_server, pbs_errno, pbs_strerror(pbs_errno));
    }

    /* However, fully qualified JobIDs need the fully qualified server name.
     * We can get it by calling pbs_statserver.
     */
    if( (status = pbs_statserver(connect, &a_servername, NULL)) == NULL ){
        errx("%p: Failed to retrieve fully qualified servername for torque server.\n");
    }
    else {
        strncpy(servername, status->name, PBS_MAXSERVERNAME);
        pbs_statfree(status);
    }

    if( joblist != NULL ){
        /* Add the nodes for the jobs provided with the -j flag.
         */

        li = list_iterator_create(joblist);

        while( (p = list_next(li)) ){
            /* The provided jobids are expected to be "integer only" jobids
             * so the fully qualified server name must be appended
             */
            _create_fq_jobid(jobid, p, servername);
            hl = _add_jobnodes(hl, connect, jobid);
        }
	list_iterator_destroy(li);
    }
    else if( envjobid != NULL ) {
        /* The env variable PBS_JOBID is expected to be a fully qualified jobid,
         * hence _create_fq_jobid is not called.
	 */
        hl = _add_jobnodes(hl, connect, envjobid);
    }

    if( pbs_disconnect(connect) ){
        errx("%p: Failed to disconnect from torque server %s: (%d) %s\n",
             pbs_server, pbs_errno, pbs_strerror(pbs_errno));
    }

    if (hl)
        hostlist_uniq (hl);
    return (hl);
}

/*
 * vi: tabstop=4 shiftwidth=4 expandtab
 */

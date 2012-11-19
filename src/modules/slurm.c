/*****************************************************************************\
 *  $Id$
 *****************************************************************************
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

#include "src/common/hostlist.h"
#include "src/common/split.h"
#include "src/common/err.h"
#include "src/common/xmalloc.h"
#include "src/common/xstring.h"
#include "src/pdsh/xpopen.h"
#include "src/pdsh/ltdl.h"
#include "src/pdsh/mod.h"
#include "src/pdsh/opt.h"

/*
 * SLURM headers need to be included after pdsh header files to
 *  avoid possibly conflicts with the definition of "bool"
 */
#include <slurm/slurm.h>
#include <slurm/slurm_errno.h>

#if STATIC_MODULES
#  define pdsh_module_info slurm_module_info
#  define pdsh_module_priority slurm_module_priority
#endif    
/*
 *  Give this module low priority 
 */
int pdsh_module_priority = 10;


/*
 *  Call this module after all option processing. The module will only
 *    try to read the SLURM_JOBID if opt->wcoll is not already set.
 *    Calling the module in postop allows us to be sure that all other
 *    modules had a chance to update the wcoll.
 */
static int mod_slurm_init(void);
static int mod_slurm_wcoll(opt_t *opt);
static int mod_slurm_exit(void);
static hostlist_t _slurm_wcoll(List jobids);
static hostlist_t _slurm_wcoll_partition(List partitions);
static int slurm_process_opt(opt_t *, int opt, char *arg);

static List job_list = NULL;
static List partition_list = NULL;

/*
 *  Export generic pdsh module options
 */
struct pdsh_module_operations slurm_module_ops = {
    (ModInitF)       mod_slurm_init, 
    (ModExitF)       mod_slurm_exit, 
    (ModReadWcollF)  mod_slurm_wcoll,
    (ModPostOpF)     NULL
};


/* 
 * Export module options
 */
struct pdsh_module_option slurm_module_options[] = 
 { 
   { 'j', "jobid,...", 
     "Run on nodes allocated to SLURM job(s) (\"all\" = all jobs)",
     DSH | PCP, (optFunc) slurm_process_opt
   },
   { 'P', "partition,...",
     "Run on nodes contained in SLURM partition",
     DSH | PCP, (optFunc) slurm_process_opt
   },
   PDSH_OPT_TABLE_END
 };

/* 
 * SLURM module info 
 */
struct pdsh_module pdsh_module_info = {
  "misc",
  "slurm",
  "Mark Grondona <mgrondona@llnl.gov>",
  "Target nodes contained in SLURM jobs or partitions, read SLURM_JOBID by default",
  DSH | PCP, 

  &slurm_module_ops,
  NULL,
  &slurm_module_options[0],
};


static int mod_slurm_init (void)
{
    return (0);
}


static int32_t str2jobid (char *str)
{
    char *p = NULL;
    long int jid;

    if (str == NULL) 
        return (-1);

    jid = strtoul (str, &p, 10);

    if (*p != '\0') 
        errx ("%p: invalid setting \"%s\" for -j or SLURM_JOBID\n", str);

    return ((int32_t) jid);
}

    
static int
slurm_process_opt(opt_t *pdsh_opts, int opt, char *arg)
{
    switch (opt) {
    case 'j':
        job_list = list_split_append (job_list, ",", arg);
        break;
    case 'P':
        partition_list = list_split_append (partition_list, ",", arg);
        break;
    default:
        break;
    }

    return (0);
}

static int
mod_slurm_exit(void)
{
    if (job_list)
        list_destroy (job_list);

    if (partition_list)
        list_destroy (partition_list);

    return (0);
}

/*
 *  If no wcoll has been established by this time, look for the
 *    SLURM_JOBID env var, and set wcoll to the list of nodes allocated
 *    to that job.
 */
static int mod_slurm_wcoll(opt_t *opt)
{
    if (job_list && opt->wcoll)
        errx("%p: do not specify -j with any other node selection option.\n");

    if (partition_list && opt->wcoll)
        errx("%p: do not specify -P with any other node selection option.\n");

    if (partition_list && job_list)
        errx("%p: do not specify -j and -P together.\n");

    if (partition_list)
        opt->wcoll = _slurm_wcoll_partition (partition_list);

    if (!opt->wcoll) 
            opt->wcoll = _slurm_wcoll (job_list); 

    return 0;
}

static int32_t _slurm_jobid (void)
{
    return (str2jobid (getenv ("SLURM_JOBID")));
}

static int _find_id (char *jobid, uint32_t *id)
{
    return (*id == str2jobid (jobid));
}

static int _find_str (char *jobid, char *str)
{
    return (strcmp (jobid, str) == 0);
}

/*
 * Return non-zero if jobid is in list of ids requested by user
 */
static int _jobid_requested (List l, uint32_t jobid)
{
    if (l == NULL)
        return (0);
    return (list_delete_all (l, (ListFindF)_find_id, &jobid));
}

static int _partition_requested (List l, char *partition)
{
    if (l == NULL)
        return (0);
    return (list_delete_all (l, (ListFindF)_find_str, partition));
}

static int _alljobids_requested (List l)
{
    char *all = "all";
    if (l == NULL)
        return (0);
    return (list_delete_all (l, (ListFindF)_find_str, all));
}

static hostlist_t _hl_append (hostlist_t hl, char *nodes)
{
    if (hl == NULL)
        return (hostlist_create (nodes));
    else
        hostlist_push (hl, nodes);
    return (hl);
}

static hostlist_t _slurm_wcoll (List joblist)
{
    int i;
    hostlist_t hl = NULL;
    job_info_msg_t * msg;
    int32_t envjobid = 0;
    int alljobids = 0;

    if ((joblist == NULL) && (envjobid = _slurm_jobid()) < 0)
        return (NULL);

    if (slurm_load_jobs((time_t) NULL, &msg, 1) < 0) 
        errx ("Unable to contact slurm controller: %s\n", 
              slurm_strerror (errno));

    /*
     *  Check for "all" in joblist
     */
    alljobids = _alljobids_requested (joblist);

    for (i = 0; i < msg->record_count; i++) {
        job_info_t *j = &msg->job_array[i];

        if (alljobids && j->job_state == JOB_RUNNING)
            hl = _hl_append (hl, j->nodes);
        else if (!joblist && (j->job_id == envjobid)) {
            /*
             *  Only use SLURM_JOBID environment variable if user
             *   didn't override with -j option
             */
            hl = hostlist_create (j->nodes);
            break;
        }
        else if (_jobid_requested (joblist, j->job_id)) {
            hl = _hl_append (hl, j->nodes);
            /* 
             * Exit when there is no more jobids to search
             */
            if (list_count (joblist) == 0)
                break;
        }
    }
    
    slurm_free_job_info_msg (msg);

    if (hl)
        hostlist_uniq (hl);

    return (hl);
}

static hostlist_t _slurm_wcoll_partition (List partitionlist)
{
    int i;
    char * str;
    hostlist_t hl = NULL;
    partition_info_msg_t * msg;
    partition_info_t * p;
    ListIterator li;

    if (slurm_load_partitions((time_t) NULL, &msg, 1) < 0)
        errx ("Unable to contact slurm controller: %s\n",
              slurm_strerror (errno));

    for (i = 0; i < msg->record_count; i++){
        p = &msg->partition_array[i];

        if (_partition_requested (partitionlist, p->name)) {
            hl = _hl_append (hl, p->nodes);
            /*
             * Exit when there is no more partitions to search
             */
            if (list_count (partitionlist) == 0)
                break;
        }
    }

    /*
     *  Anything left in partitionlist wasn't found, emit a warning
     */
    li = list_iterator_create(partitionlist);
    while ((str = list_next(li))){
       err("%p: Warning - partition %s not found\n", str);
    }

    slurm_free_partition_info_msg (msg);

    if (hl)
        hostlist_uniq (hl);

    return (hl);
}

/*
 * vi: tabstop=4 shiftwidth=4 expandtab
 */

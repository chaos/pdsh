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

#if     HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <paths.h>
#include <stdarg.h>
#include <ctype.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>             /* INT_MAX */
#include <pthread.h>

#if HAVE_LIBELANCTRL
#  include <elan/elanctrl.h>
#  include <elan/capability.h>

#  define HighNode    cap_highnode
#  define LowNode     cap_lownode
#  define HighContext cap_highcontext
#  define LowContext  cap_lowcontext
#  define Bitmap      cap_bitmap
#  define Type        cap_type
#  define UserKey     cap_userkey
#  define RailMask    cap_railmask
#  define Values      key_values

/* We need these using the old libelan3 library calls
 *  so we redefine them to old values here.
 *  XXX: What is the equivalent for libelanctrl?
 */
#  define ELAN_USER_BASE_CONTEXT_NUM 0x020
#  define ELAN_USER_TOP_CONTEXT_NUM  0x7ff

#include <sys/stat.h>

#elif HAVE_LIBELAN3
#  include <elan3/elan3.h>
#  include <elan3/elanvp.h>
#else
#  error "Need either libelan3 or libelanctrl to compile this module."
#endif

#include <rms/rmscall.h>

#include <dlfcn.h>

#include "src/common/xmalloc.h"
#include "src/common/xstring.h"
#include "src/common/hostlist.h"
#include "src/common/list.h"
#include "src/common/err.h"
#include "qswutil.h"
#include "elanhosts.h"

/* we will allocate program descriptions in this range */
/* XXX note: do not start at zero as libelan shifts to get unique shm id */
#define QSW_PRG_START  1
#define QSW_PRG_END    INT_MAX

static int debug_syslog = 1;    /* syslog program setup at LOG_DEBUG level */

/*
 *  Static "Elan Host" configuration
 */
static elanhost_config_t elanconf = NULL;


/* 
 *  Static function prototypes:
 */
static int _set_elan_ids(elanhost_config_t ec);
static void *neterr_thr(void *arg);


int qsw_init(void)
{
    assert(elanconf == NULL);

    elanconf = elanhost_config_create();

    if (elanhost_config_read(elanconf, NULL) < 0) {
        err("%p: error: %s\n", elanhost_config_err(elanconf));
        return -1;
    }

    return 0;
}

void qsw_fini(void)
{
    elanhost_config_destroy(elanconf);
}

static int qsw_have_elan3(void)
{
#if HAVE_LIBELAN3
    return (1);
#else
    struct stat st;

    if (stat("/proc/qsnet/elan3", &st) < 0)
        return (0);

    return (1);
#endif /* HAVE_LIBELAN3 */
    return (0);
}

struct neterr_args {
    pthread_mutex_t *mutex;
    pthread_cond_t  *cond;
    int             neterr_rc;
};

int qsw_spawn_neterr_thr(void)
{
    struct neterr_args args;
    pthread_attr_t attr;
    pthread_t neterr_tid;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t  cond  = PTHREAD_COND_INITIALIZER;

    /* 
     * Only need to run neterr thread on Elan3 HW.
     */
    if (!qsw_have_elan3()) 
        return (0);

    args.mutex = &mutex;
    args.cond  = &cond;

    if ((errno = pthread_attr_init(&attr)))
        errx("%p: pthread_attr_init: %m\n");

    errno = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (errno)
        err("%p: pthread_attr_setdetachstate: %m");

    pthread_mutex_lock(&mutex);

    if ((errno = pthread_create(&neterr_tid, &attr, neterr_thr, &args)))
        return -1;

    /*
     *  Wait for successful startup of neterr resolver thread before 
     *    returning control to main thread.
     */
    pthread_cond_wait(&cond, &mutex);
    pthread_mutex_unlock(&mutex);

    return args.neterr_rc;

    return 0;
}

/*
 * Use dlopen () for libelan3.so (when needed)
 *   This allows us to build a single version of the qsnet modules
 *   for Elan3 and Elan4 QsNetII systems.
 */

/* 
 * libelan3.so handle:
 */
static void * elan3h = NULL; 

/*
 * Wrapper functions for needed libelan3 functions
 */
static int _elan3_init_neterr_svc (int dbglvl)
{
    static int (*init_svc) (int);

    if (!(init_svc = dlsym (elan3h, "elan3_init_neterr_svc"))) 
        return (0);

    return (init_svc (dbglvl));
}


static int _elan3_register_neterr_svc (void)
{
    static int (*reg_svc) (void);

    if (!(reg_svc = dlsym (elan3h, "elan3_register_neterr_svc"))) 
        return (0);

    return (reg_svc ());
}

static int _elan3_run_neterr_svc (void)
{
    static int (*run_svc) ();

    if (!(run_svc = dlsym (elan3h, "elan3_run_neterr_svc"))) 
        return (0);

    return (run_svc ());
}


static int _elan3_load_neterr_svc (int i, char *host)
{
    static int (*load_svc) (int, char *);

    if (!(load_svc = dlsym (elan3h, "elan3_load_neterr_svc"))) 
        return (0);

    return (load_svc (i, host));
}


static int
_set_elan_ids(elanhost_config_t ec)
{
    int i;
    for (i = 0; i <= elanhost_config_maxid(ec); i++) {
        char *host = elanhost_elanid2host(ec, ELANHOST_EIP, i);
        if (!host)
            continue;
        
		if (_elan3_load_neterr_svc(i, host) < 0)
			err("%p: elan3_load_neterr_svc(%d, %s): %m", i, host);
	}

    return 0;
}

static void *neterr_thr(void *arg)
{	
    struct neterr_args *args = arg;

    if (!(elan3h = dlopen ("libelan3.so", RTLD_LAZY))) {
        syslog(LOG_ERR, "unable to open libelan3.so: %s", dlerror());
        goto fail;
    }

	if (!_elan3_init_neterr_svc(0)) {
		syslog(LOG_ERR, "elan3_init_neterr_svc: %m");
		goto fail;
	}

	/* 
	 *  Attempt to register the neterr svc thread. If the address 
	 *   cannot be bound, then there is already a thread running, and
	 *   we should just exit with success.
	 */
	if (!_elan3_register_neterr_svc()) {
		if (errno != EADDRINUSE) {
			syslog(LOG_ERR, "elan3_register_neterr_svc: %m");
			goto fail;
		}
        /* error resolver already running, just return */
        goto done;
	}

    /* 
     * Attempt to register elan ids with kernel if we successfully 
     *  registered the error resolver service.
     */
    _set_elan_ids(elanconf);

   done:
	/* 
	 *  Signal main thread that we've successfully initialized
	 */
	pthread_mutex_lock(args->mutex);
	args->neterr_rc = 0;
	pthread_cond_signal(args->cond);
	pthread_mutex_unlock(args->mutex);

	/*
	 *  Run the network error resolver thread. This should
	 *   never return. If it does, there's not much we can do
	 *   about it.
	 */
	_elan3_run_neterr_svc();

    return NULL;

   fail:
	pthread_mutex_lock(args->mutex);
	args->neterr_rc = -1;
	pthread_cond_signal(args->cond);
	pthread_mutex_unlock(args->mutex);

	return NULL;
}

static void
_free_it (void *item)
{
    Free((void **) &item);
}

static List
_hostlist_to_elanids (hostlist_t nodelist)
{
    char *host = NULL;
    List l = list_create ((ListDelF) _free_it);
    hostlist_iterator_t i = hostlist_iterator_create (nodelist);

    if (l == NULL)
        errx ("%p: list_create: %m");

    if (i == NULL)
        errx ("%p: hostlist_iterator_create: %m");

    while ((host = hostlist_next (i))) {
        int *id = Malloc (sizeof(int));
        
        if ((*id = elanhost_host2elanid (elanconf, host)) < 0) {
            err ("%p: Unable to get ElanId for \"%s\": %s\n", 
                 host, elanhost_config_err (elanconf));
            goto fail;
        }

        list_append (l, id);
        free (host);
    }
    hostlist_iterator_destroy (i);

    return (l);

  fail: 
    if (host != NULL)
        free (host);
    if (i != NULL)
        hostlist_iterator_destroy (i);
    if (l != NULL)
        list_destroy (l);

    return (NULL);
}

static int
_elanid_min (List el)
{
    int *id;
    int  min = -1;
    ListIterator i = list_iterator_create (el);

    while ((id = list_next (i))) {
        if ((*id < min) || (min == -1))
            min = *id;
    }

    list_iterator_destroy (i);

    return (min);
}

static int
_elanid_max (List el)
{
    int *id;
    int  max = -1;
    ListIterator i = list_iterator_create (el);

    while ((id = list_next (i))) {
        if ((*id > max) || (max == -1))
            max = *id;
    }

    list_iterator_destroy (i);

    return (max);
}


/*
 * Given a list of hostnames and the number of processes per node, 
 * set the correct bits in the capability's bitmap and set high and
 * low node id's.
 */
static int
_setbitmap(hostlist_t nodelist, int procs_per_node, int cyclic, 
           ELAN_CAPABILITY * cap)
{
    int *id;
    int nodes_in_bitmap;
    int rc = 0;
    List el;
    ListIterator itr;

    if (!(el = _hostlist_to_elanids (nodelist)))
        return (-1);

    cap->HighNode = _elanid_max (el);
    cap->LowNode  = _elanid_min (el);

    if (cap->HighNode == -1 || cap->LowNode == -1)
        return -1;

    nodes_in_bitmap = cap->HighNode - cap->LowNode + 1;

    /*
     * There are (procs_per_node * nnodes) significant bits in the mask, 
     * each representing a process slot.  Bits are off where for holes 
     * corresponding to process slots for unallocated nodes.
     * For example, if nodes 4 and 6 are running two processes per node,
     * bits 0,1 (corresponding to the two processes on node 4) and bits 4,5
     * (corresponding to the two processes running no node 6) are set.
     *
     * Note that for QsNet, the bits have a different meaning depending
     * on whether the capability distribution type is cyclic or block.
     * For block distribution, the bits are laid out in node-major
     * format, while for cyclic distribution, a procid (or context) major
     * format is used. 
     * 
     * Example: 2 processes per node on nodes 0,2:
     *
     *        block                       cyclic
     *                                      
     *    2  |  1  |  0     NodeId     2 1 0 | 2 1 0
     *       |     |                         |
     *   1 0 | 1 0 | 1 0   ContextId     1   |   0         
     *       |     |                         |       
     *   5 4 | 3 2 | 1 0  Bit Numbers  5 4 3 | 2 1 0
     *       |     |                         |
     *  ---- +-----+-----             -------+-------
     *   1 1 | 0 0 | 1 1   Bit Value   1 0 1 | 1 0 1
     */

    itr = list_iterator_create (el);

    while ((id = list_next (itr))) {
        int node = (*id) - cap->LowNode; /* relative id w/in bitmap */
        int i;

        for (i = 0; i < procs_per_node; i++) {
            int bit;
            if (cyclic) 
                bit = (i * nodes_in_bitmap) + node;
            else
                bit = (node * (procs_per_node)) + i;

            if (bit >= (sizeof (cap->Bitmap) * 8)) {
                err ("%p: _setbitmap: bit %d out of range\n", bit);
                rc = -1;
                break;
            }

            BT_SET(cap->Bitmap, bit);
        }
    }
    list_destroy (el);

    return (rc);
}

/*
 * Set a variable in the callers environment.  Args are printf style.
 * XXX Space is allocated on the heap and will never be reclaimed.
 * Example: setenvf("RMS_RANK=%d", rank);
 */
static int _setenvf(const char *fmt, ...)
{
    va_list ap;
    char buf[BUFSIZ];
    char *bufcpy;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    bufcpy = strdup(buf);
    if (bufcpy == NULL)
        return -1;
    return putenv(bufcpy);
}

static int _rms_setenv(qsw_info_t * qi)
{
    /* MPI wants some of these ... 
     *  (It doesn't anymore, but they are helpful when running
     *   parallel scripts - ashley@quadrics.com )
     */
    if (_setenvf("RMS_RANK=%d", qi->rank) < 0)
        return -1;
    if (_setenvf("RMS_NODEID=%d", qi->nodeid) < 0)
        return -1;
    if (_setenvf("RMS_PROCID=%d", qi->procid) < 0)
        return -1;
    if (_setenvf("RMS_NNODES=%d", qi->nnodes) < 0)
        return -1;
    if (_setenvf("RMS_NPROCS=%d", qi->nprocs) < 0)
        return -1;

    if (_setenvf("ELAN_AUTO=pdsh") < 0)
        return -1;
    if (_setenvf("ELAN_JOBID=%d", qi->prgnum) < 0)
        return -1;

#if 0
    /* I'm not sure what this should be set to yet,
     * libelan will do the right thing if it's not
     * set though. (ashley@quadrics.com) */
    if (_setenvf("LIBELAN_SHMKEY=%d", qi->prgnum) < 0)
        return -1;
#endif 

    return 0;
}

/*
 * Return the number of times qsw_encode_cap_bitamp/qsw_decode_cap_bitmap
 * must be called.
 */
int qsw_cap_bitmap_count(void)
{
    ELAN_CAPABILITY cap;
    int count = sizeof(cap.Bitmap) / sizeof(cap.Bitmap[0]);

    assert(count % 16 == 0);
    return count;
}

/*
 * Convert capability (all but cap->Bitmap) to string.
 */
int qsw_encode_cap(char *s, int len, ELAN_CAPABILITY * cap)
{
    int n;

    if (sizeof(cap->UserKey.Values[0]) != 4) {
        err("%p: qsw_encode_cap: UserKey is unexpected size\n");
        return -1;
    }
    if (sizeof(cap->UserKey) / 4 != 4) {
        err("%p: qsw_encode_cap: UserKey array is unexpected size\n");
        return -1;
    }
#if HAVE_LIBELANCTRL
    cap->cap_spare = ELAN_CAP_UNINITIALISED ;
    n = snprintf(s, len, "%x.%x.%x.%x.%hx.%x.%x.%x.%x.%x.%x.%x",
                           cap->UserKey.Values[0],
                           cap->UserKey.Values[1], 
                           cap->UserKey.Values[2], 
                           cap->UserKey.Values[3], 
                           cap->Type, /* short */
#ifdef ELAN_CAP_ELAN3
                           cap->cap_elan_type, /* char */
#else
                           cap->cap_spare,
#endif
                           cap->LowContext,
                           cap->HighContext,
                           cap->cap_mycontext,
                           cap->LowNode,
                           cap->HighNode,
                           cap->RailMask);
#elif HAVE_LIBELAN3
    n = snprintf(s, len, "%x.%x.%x.%x.%hx.%x.%x.%x.%x.%x.%x.%x",
                           cap->UserKey.Values[0],
                           cap->UserKey.Values[1],
                           cap->UserKey.Values[2],
                           cap->UserKey.Values[3],
                           cap->Type,      /* short */
                           cap->LowContext,
                           cap->HighContext,
                           cap->MyContext,
                           cap->LowNode,
                           cap->HighNode,
                           cap->Entries,
                           cap->RailMask);

#else
#error "Neither LIBELAN3 nor LIBELANCTRL defined!"
#endif

    if (n < 0 || n > strlen(s)) {
        err("%p: qsw_encode_cap: string overflow\n");
        return -1;
    }
    return 0;
}

/*
 * Convert cap->Bitmap to string.
 */
int qsw_encode_cap_bitmap(char *s, int len, ELAN_CAPABILITY * cap, int i)
{
    int n;

    if (sizeof(cap->Bitmap[0]) != sizeof(unsigned int)) {
        err("%p: qsw_encode_cap_bitmap: Bitmap is unexpected size\n");
        return -1;
    }
    if ((sizeof(cap->Bitmap) / sizeof(cap->Bitmap[0])) % 16 != 0) {
        err("%p: qsw_encode_cap_bitmap: Bitmap is not mult of 16\n");
        return -1;
    }
    if (i < 0 || i >= (sizeof(cap->Bitmap) / sizeof(cap->Bitmap[0]))) {
        err("%p: qsw_encode_cap_bitmap: Bitmap index out of range\n");
        return -1;
    }
    n = snprintf(s, len, "%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x",
                 cap->Bitmap[i + 0], cap->Bitmap[i + 1],
                 cap->Bitmap[i + 2], cap->Bitmap[i + 3],
                 cap->Bitmap[i + 4], cap->Bitmap[i + 5],
                 cap->Bitmap[i + 6], cap->Bitmap[i + 7],
                 cap->Bitmap[i + 8], cap->Bitmap[i + 9],
                 cap->Bitmap[i + 10], cap->Bitmap[i + 11],
                 cap->Bitmap[i + 12], cap->Bitmap[i + 13],
                 cap->Bitmap[i + 14], cap->Bitmap[i + 15]);
    if (n == -1 || n > strlen(s)) {
        err("%p: qsw_encode_cap_bitmap: string overflow\n");
        return -1;
    }
    return 0;
}

/*
 * Convert string to capability (all but cap->Bitmap).
 */
int qsw_decode_cap(char *s, ELAN_CAPABILITY * cap)
{
    int n;

#if HAVE_LIBELANCTRL
    /* initialize capability */
    elan_nullcap(cap);

    n =  sscanf(s, "%x.%x.%x.%x.%hx.%hx.%x.%x.%x.%x.%x.%x",
                     &cap->UserKey.Values[0],
                     &cap->UserKey.Values[1],
                     &cap->UserKey.Values[2],
                     &cap->UserKey.Values[3],
                     &cap->cap_type,      /* short */
#  ifdef ELAN_CAP_ELAN3
                     &cap->cap_elan_type, /* char */
#  else
                     &cap->cap_spare,     /* unsigned short */
#  endif
                     &cap->LowContext,
                     &cap->HighContext,
                     &cap->cap_mycontext,
                     &cap->LowNode,
                     &cap->HighNode,
                     &cap->RailMask);

#elif HAVE_LIBELAN3

    /* initialize capability */
    elan3_nullcap(cap);

    /* fill in values sent from remote */
    n = sscanf(s, "%x.%x.%x.%x.%hx.%x.%x.%x.%x.%x.%x.%x", 
                    &cap->UserKey.Values[0], 
                    &cap->UserKey.Values[1],
                    &cap->UserKey.Values[2],
                    &cap->UserKey.Values[3],
                    &cap->Type, /* short */
                    &cap->LowContext,
                    &cap->HighContext,
                    &cap->MyContext,
                    &cap->LowNode,
                    &cap->HighNode, 
                    &cap->Entries, 
                    &cap->RailMask);
#else
#  error "Neither LIBELANCTRL nor LIBELAN3 set!"
#endif

    if (n != 12) {
        err("%p: qsw_decode_cap: scan error (%d of %d)\n", n, 12);
        return -1;
    }
    return 0;
}

/*
 * Convert string to cap->Bitmap.
 */
int qsw_decode_cap_bitmap(char *s, ELAN_CAPABILITY * cap, int i)
{
    int n;

    if (i < 0 || i >= sizeof(cap->Bitmap) / sizeof(cap->Bitmap[0])) {
        err("%p: qsw_decode_cap_bitmap: BitMap index out of range\n");
        return -1;
    }
    n = sscanf(s, "%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x",
               &cap->Bitmap[i + 0], &cap->Bitmap[i + 1],
               &cap->Bitmap[i + 2], &cap->Bitmap[i + 3],
               &cap->Bitmap[i + 4], &cap->Bitmap[i + 5],
               &cap->Bitmap[i + 6], &cap->Bitmap[i + 7],
               &cap->Bitmap[i + 8], &cap->Bitmap[i + 9],
               &cap->Bitmap[i + 10], &cap->Bitmap[i + 11],
               &cap->Bitmap[i + 12], &cap->Bitmap[i + 13],
               &cap->Bitmap[i + 14], &cap->Bitmap[i + 15]);
    if (n != 16) {
        err("%p: qsw_decode_cap_bitmap(%d): scan error\n", i);
        return -1;
    }
    return 0;
}

/*
 * string -> info
 */
int qsw_decode_info(char *s, qsw_info_t * qi)
{
    int n;

    n = sscanf(s, "%x.%x.%x.%x.%x.%x",
               &qi->prgnum,
               &qi->rank,
               &qi->nodeid, &qi->procid, &qi->nnodes, &qi->nprocs);
    if (n != 6) {
        err("%p: qsw_decode_info: scan error\n");
        return -1;
    }
    return 0;
}

/*
 * info -> string
 */
int qsw_encode_info(char *s, int len, qsw_info_t * qi)
{
    int n;

    n = snprintf(s, len, "%x.%x.%x.%x.%x.%x",
                 qi->prgnum,
                 qi->rank, qi->nodeid, qi->procid, qi->nnodes, qi->nprocs);
    if (n == -1 || n > strlen(s)) {
        err("%p: qsw_encode_info: string overflow\n");
        return -1;
    }
    return 0;
}

/*
 * Generate a random program number.  Normally these would be allocated,
 * but since we have no persistant daemon, we settle for random.
 * Must be called after qsw_init_capability (we seed lrand48 there).
 */
int qsw_get_prgnum(void)
{
    int prgnum;

    prgnum = lrand48() % (QSW_PRG_END - QSW_PRG_START + 1);
    prgnum += QSW_PRG_START;

    return prgnum;
}

/*
 * Prepare a capability that will be passed to all the processes in a 
 * parallel program.
 * Function returns a 0 on success, -1 = fail.
 */
int
qsw_init_capability(ELAN_CAPABILITY * cap, int nprocs, hostlist_t nodelist,
                    int cyclic_alloc)
{
    int i;
    int num_nodes = hostlist_count(nodelist);
    int procs_per_node = nprocs / num_nodes;


    srand48(getpid());

    /*
     * Initialize for multi rail and either block or cyclic allocation.  
     * Set ELAN_CAP_TYPE_BROADCASTABLE later if appropriate.
     */
#if HAVE_LIBELANCTRL
    elan_nullcap(cap);
#elif HAVE_LIBELAN3
    elan3_nullcap(cap);
#else
#  error
#endif

    if (cyclic_alloc)
        cap->Type = ELAN_CAP_TYPE_CYCLIC;
    else
        cap->Type = ELAN_CAP_TYPE_BLOCK;
    cap->Type |= ELAN_CAP_TYPE_MULTI_RAIL;
    cap->RailMask = 1;

#if HAVE_LIBELANCTRL
#  ifdef ELAN_CAP_ELAN3
    cap->cap_elan_type = ELAN_CAP_ELAN3;
#  else
    cap->cap_spare = ELAN_CAP_UNINITIALISED;
#  endif
#endif

    /*
     * UserKey is 128 bits of randomness which should be kept private.
     */
    for (i = 0; i < 4; i++)
        cap->UserKey.Values[i] = lrand48();

    /*
     * Elan hardware context numbers must be unique per node.
     * One is allocated to each parallel process.  In order for processes 
     * on the same node to communicate, they must use contexts in the 
     *  hi-lo range of a common capability.  With pdsh we have no 
     * persistant daemon to allocate these, so we settle for a random one.  
     */
    cap->LowContext = lrand48() %
        (ELAN_USER_TOP_CONTEXT_NUM -
         (ELAN_USER_BASE_CONTEXT_NUM + procs_per_node - 1) - 1);
    cap->LowContext += ELAN_USER_BASE_CONTEXT_NUM;
    cap->HighContext = cap->LowContext + procs_per_node - 1;
    /* not necessary to initialize cap->MyContext */

    /*
     * Describe the mapping of processes to nodes.
     * This sets cap->HighNode, cap->LowNode, and cap->Bitmap.
     */
    if (_setbitmap(nodelist, procs_per_node, cyclic_alloc, cap) < 0) {
        err("%p: do all target nodes have an Elan adapter?\n");
        return -1;
    }

#if HAVE_LIBELAN3
    /* 
     * Set cap->Entries and add broadcast bit to cap->type based on 
     * cap->HighNode and cap->LowNode values set above.
     */
    cap->Entries = nprocs;
    if (cap->Entries > ELAN_MAX_VPS) {
        err("%p: program would have too many processes (max %d)\n",
                ELAN_MAX_VPS);
        return -1;
    }
#endif

    /* 
     * As we now support segmented broadcast, always flag the capability
     * as broadcastable. 
     */
    /*if (abs(cap->HighNode - cap->LowNode) == num_nodes - 1) */
    cap->Type |= ELAN_CAP_TYPE_BROADCASTABLE;

    return 0;
}

/*
 * Take necessary steps to set up to run an Elan MPI "program" 
 * (set of processes) on a node.  
 *
 * Process 1	Process 2	|	Process 3
 * read args			|
 * fork	-------	rms_prgcreate	|
 * waitpid 	elan3_create	|
 * 		rms_prgaddcap	|
 *		fork N procs ---+------	rms_setcap
 *		wait all	|	setup RMS_ env	
 *				|	setuid, etc.
 *				|	exec mpi process
 *		exit		|
 * rms_prgdestroy		|
 * exit				|     (one pair of processes per mpi proc!)
 *
 * Explanation of the two fork(2) calls:
 * - The first fork is required because rms_prgdestroy can't occur in the 
 *   process that calls rms_prgcreate (since it is a member, ECHILD).
 * - The second fork is required when running multiple processes per node 
 *   because each process must announce its use of one of the hw contexts 
 *   in the range allocated in the capability.
 *
 * One process:
 *    init-xinetd-+-in.qshd---in.qshd---in.qshd---sleep
 * Two processes:
 *    init-xinetd-+-in.qshd---in.qshd---2*[in.qshd---sleep]
 * (if stderr backchannel is active, add one in.qshd)
 *   
 * Any errors result in a message on stderr and program exit.
 */
void qsw_setup_program(ELAN_CAPABILITY * cap, qsw_info_t * qi, uid_t uid)
{
    int pid;
    int cpid[ELAN_MAX_VPS];
    int procs_per_node;
    int proc_index;

    if (qi->nprocs > ELAN_MAX_VPS)      /* should catch this in client */
        errx("%p: too many processes requested\n");

    /* 
     * First fork.  Parent waits for child to terminate, then cleans up.
     */
    pid = fork();
    switch (pid) {
    case -1:                   /* error */
        errx("%p: fork: %m\n");
    case 0:                    /* child falls thru */
        break;
    default:                   /* parent */
        if (waitpid(pid, NULL, 0) < 0)
            errx("%p: waitpid: %m\n");
        while (rms_prgdestroy(qi->prgnum) < 0) {
            if (errno != ECHILD)
                errx("%p: rms_prgdestroy: %m\n");
            sleep(1);           /* waitprg would be nice! */
        }
        exit(0);
    }
    /* child continues here */

    /* 
     * Set up capability 
     */
    {
        int i, nrails;
#if HAVE_LIBELANCTRL
        /* MULTI-RAIL: Extract rail info from capability */
        nrails = elan_nrails(cap);

        /* MULTI-RAIL: Create the capability in all rails */
        for (i = 0; i < nrails; i++) {
            ELANCTRL_HANDLE handle;

            /* 
             * Open up the Elan control device so we can create 
             * a new capability.  
             */
            if (elanctrl_open(&handle) != 0)
                errx("%p: elanctrl_open(): %m\n");

            /* Push capability into device driver */
            if (elanctrl_create_cap(handle, cap) < 0)
                errx("%p: elancrtl_create_cap failed: %m\n");

        }

#elif HAVE_LIBELAN3
        /* MULTI-RAIL: Extract rail info from capability */
        nrails = elan3_nrails(cap);

        /* MULTI-RAIL: Create the capability in all rails */
        for (i = 0; i < nrails; i++) {
            ELAN3_CTX *ctx;

            /* 
             * Open up the control device so we can create a new 
             * capability.  This will fail if we don't have rw 
             * access to /dev/elan3/control[i]
             */
            if ((ctx = elan3_control_open(i)) == NULL)
                errx("%p: elan3_control_open(%d): %m\n", i);

            /* Push capability into device driver */
            if (elan3_create(ctx, cap) < 0)
                errx("%p: elan3_create failed: %m\n");
        }
#else
#  error
#endif
    }

    /* associate this process and its children with prgnum */
    if (rms_prgcreate(qi->prgnum, uid, 1) < 0)  /* 1 cpu (bogus!) */
        errx("%p: rms_prgcreate %d failed: %m\n", qi->prgnum);

    /* make cap known via rms_getcap/rms_ncaps to members of this prgnum */
    if (rms_prgaddcap(qi->prgnum, 0, cap) < 0)
        errx("%p: rms_prgaddcap failed: %m\n");

    if (debug_syslog) {
        char tmpstr[1024];

        syslog(LOG_DEBUG, "prg %d cap %s bitmap 0x%.8x", qi->prgnum,
#if HAVE_LIBELANCTRL
                elan_capability_string(cap, tmpstr),
#elif HAVE_LIBELAN3
                elan3_capability_string(cap, tmpstr), 
#endif
                cap->Bitmap[0]);
    }

    /* 
     * Second fork - once for each process.
     * Parent waits for all children to exit the it exits.
     * Child assigns hardware context to each process, then forks again...
     */
    procs_per_node = qi->nprocs / qi->nnodes;
    for (proc_index = 0; proc_index < procs_per_node; proc_index++) {
        cpid[proc_index] = fork();
        if (cpid[proc_index] < 0)
            errx("%p: fork (%d): %m\n", proc_index);
        else if (cpid[proc_index] == 0)
            break;
    }
    /* parent */
    if (proc_index == procs_per_node) {
        int waiting = procs_per_node;
        int i;

        while (waiting > 0) {
            pid = waitpid(0, NULL, 0);  /* any in pgrp */
            if (pid < 0)
                errx("%p: waitpid: %m\n");
            for (i = 0; i < procs_per_node; i++) {
                if (cpid[i] == pid)
                    waiting--;
            }
        }
        exit(0);
    }
    /* child falls through here */
    /* proc_index will be set to the child's index */

    /*
     * Assign elan hardware context to current process.
     * - arg1 is an index into the kernel's list of caps for this 
     *   program desc (added by rms_prgaddcap).  There will be
     *   one per rail.
     * - arg2 indexes the hw ctxt range in the capability
     *   [cap->LowContext, cap->HighContext]
     */
    if (rms_setcap(0, proc_index) < 0)
        errx("%p: rms_setcap (%d): %m\n", proc_index);

    /* set RMS_ environment vars */
    switch (cap->Type & ELAN_CAP_TYPE_MASK) {
        case ELAN_CAP_TYPE_BLOCK:
            qi->procid = (qi->nodeid * procs_per_node) + proc_index;
            break;
        case ELAN_CAP_TYPE_CYCLIC:
            qi->procid = qi->nodeid + (proc_index * qi->nnodes);
            break;
        default:
            errx("%p: unsupported Elan capability type\n");
    }
    qi->rank = qi->procid;
    if (_rms_setenv(qi) < 0)
        errx("%p: failed to set environment variables: %m\n");
    /* Exec the process... */
}

int qsw_prgsignal(int prgid, int signo)
{
    return rms_prgsignal(prgid, signo);
}

#ifdef TEST_MAIN
/* encode info, then decode and check that the result is what we started with */
static void _verify_info_encoding(qsw_info_t * qi)
{
    int err;
    char tmpstr[1024];
    qsw_info_t qicpy;

    err = qsw_encode_info(tmpstr, sizeof(tmpstr), qi);
    assert(err >= 0);
    err = qsw_decode_info(tmpstr, &qicpy);
    assert(memcmp(qi, &qicpy, sizeof(qicpy)) == 0);
}

/* encode cap, then decode and check that the result is what we started with */
static void _verify_cap_encoding(ELAN_CAPABILITY * cap)
{
    ELAN_CAPABILITY capcpy;
    char tmpstr[1024];
    int err;

    err = qsw_encode_cap(tmpstr, sizeof(tmpstr), cap);
    assert(err >= 0);
    err = qsw_decode_cap(tmpstr, &capcpy);
    assert(err >= 0);
/*assert(ELAN_CAP_MATCH(&cap, &cap2)); *//* broken - see GNATS #3875 */
    assert(memcmp(cap, &capcpy, sizeof(capcpy)) == 0);
}

/* concatenate args into a single string */
static void _strncatargs(char *buf, int len, int argc, char *argv[])
{
    if (len > 0) {
        buf[0] = '\0';
    }
    while (len > 1 && argc > 0) {
        strncat(buf, argv[0], len);
        argv++;
        argc--;
        if (argc > 0)
            strncat(buf, " ", len);
    }
    buf[len - 1] = '\0';
}

static void _usage(void)
{
    errx("Usage %p [ -n procs ] [ -u uid ] command args...\n");
}

/* 
 * Test program for qsw runtime routines.  Run one or more processes locally, 
 * e.g. for MPI ping test across shared memory:
 *    qrun -n 2 -u 5588 mping 1 32768
 */
int main(int argc, char *argv[])
{
    extern char *optarg;
    extern int optind;

    char cmdbuf[1024];
    ELAN_CAPABILITY cap;
    int c;
    char *p;
    uid_t uid = 0;
    hostlist_t wcoll = hostlist_create("");
    char hostname[MAXHOSTNAMELEN];
    qsw_info_t qinfo = {
        nnodes:1,
        nprocs:1,
    };

    err_init(xbasename(argv[0]));       /* init err package */

    while ((c = getopt(argc, argv, "u:n:")) != EOF) {
        switch (c) {
        case 'u':
            uid = atoi(optarg);
            break;
        case 'n':
            qinfo.nprocs = atoi(optarg);
            break;
        default:
            _usage();
        }
    }

    argc -= optind;
    argv += optind;

    if (argc == 0)
        _usage();

    /* prep arg for the shell */
    _strncatargs(cmdbuf, sizeof(cmdbuf), argc, argv);

    /* create working collective containing only this host */
    if (gethostname(hostname, sizeof(hostname)) < 0)
        errx("%p: gethostname: %m\n");
    if ((p = strchr(hostname, '.')))
        *p = '\0';
    hostlist_push(wcoll, hostname);

    qsw_init();

    /* initialize capability for this "program" */
    if (qsw_init_capability(&cap, qinfo.nprocs / qinfo.nnodes, wcoll, 0) < 0)
        errx("%p: failed to initialize Elan capability\n");

    /* assert encode/decode routines work (we don't use them here) */
    _verify_info_encoding(&qinfo);
    _verify_cap_encoding(&cap);

    /* generate random program number */
    qinfo.prgnum = qsw_get_prgnum();

    /* set up capabilities, environment, fork, etc.. */
    qsw_setup_program(&cap, &qinfo, uid);
    /* multiple threads continue on here (one per processes) */

    if (seteuid(uid) < 0)
        errx("%p: seteuid: %m\n");
    err("%p: %d:%d executing /bin/bash -c %s\n",
        qinfo.prgnum, qinfo.procid, cmdbuf);
    execl("/bin/bash", "bash", "-c", cmdbuf, 0);
    errx("%p: exec of shell failed: %m\n");

    qsw_fini();

    exit(0);
}
#endif                          /* TEST_MAIN */

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

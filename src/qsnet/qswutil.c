/*****************************************************************************\
 *
 *  $Id$
 *  $Source$
 *
 *  Copyright (C) 1998-2002 The Regents of the University of California.
 *  Produced at Lawrence Livermore National Laboratory (cf, DISCLAIMER).
 *  Written by Jim Garlick (garlick@llnl.gov>
 *  UCRL-CODE-980038
 *  
 *  This file is part of PDSH, a parallel remote shell program.
 *  For details, see <http://www.llnl.gov/linux/pdsh/>.
 *  
 *  PDSH is free software; you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *  
 *  PDSH is distributed in the hope that it will be useful, but WITHOUT 
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License 
 *  for more details.
 *  
 *  You should have received a copy of the GNU General Public License along
 *  with PDSH; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.
\*****************************************************************************/

#if     HAVE_CONFIG_H
#include "config.h"
#endif

#if	HAVE_ELAN

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
#include <limits.h>	/* INT_MAX */

#include <elan3/elan3.h>
#include <elan3/elanvp.h>
#include <rms/rmscall.h>

#include "xmalloc.h"
#include "xstring.h"
#include "hostlist.h"
#include "qswutil.h"
#include "err.h"

/* we will allocate program descriptions in this range */
/* XXX note: do not start at zero as libelan shifts to get unique shm id */
#define QSW_PRG_START  1
#define QSW_PRG_END    INT_MAX

static int debug_syslog = 1; /* syslog program setup at LOG_DEBUG level */

/* 
 * Convert hostname to elan node number.  This version just returns
 * the numerical part of the hostname, true on our current systems.
 * Other methods such as a config file or genders attribute might be 
 * more appropriate for wider use.
 * 	host (IN)		hostname
 *	nodenum (RETURN)	elanid (-1 on failure)
 */
static int
qsw_host2elanid(char *host)
{
	char *p = host;
	int id;

	while (*p != '\0' && !isdigit(*p))
		p++;
	id = (*p == '\0') ? -1 : atoi(p);

	return id;
}

/*
 * Given a list of hostnames and the number of processes per node, 
 * set the correct bits in the capability's bitmap and set high and
 * low node id's.
 */
static int
qsw_setbitmap(hostlist_t nodelist, int procs_per_node, ELAN_CAPABILITY *cap)
{
	int node;
	char *host;
	hostlist_iterator_t itr;

	/* determine high and low node numbers */
	cap->HighNode = cap->LowNode = -1;
	if ((itr = hostlist_iterator_create(nodelist)) == NULL)
		errx("%p: hostlist_iterator_create failed\n");
	while ((host = hostlist_next(itr)) != NULL) {
		node = qsw_host2elanid(host);
		if (node < 0)
			return -1;
		if (node < cap->LowNode || cap->LowNode == -1)
			cap->LowNode = node;
		if (node > cap->HighNode || cap->HighNode == -1)
			cap->HighNode = node;
	}
	hostlist_iterator_destroy(itr);
	if (cap->HighNode == -1 || cap->LowNode == -1)
		return -1;

	/*
	 * There are (procs_per_node * nnodes) significant bits in the mask, 
	 * each representing a process slot.  Bits are off where for holes 
	 * corresponding to process slots for unallocated nodes.
	 * For example, if nodes 4 and 6 are running two processes per node,
	 * bits 0,1 (corresponding to the two processes on node 4) and bits 4,5
	 * (corresponding to the two processes running no node 6) are set.
	 */
	if ((itr = hostlist_iterator_create(nodelist)) == NULL)
		errx("%p: hostlist_iterator_create failed\n");
	while ((host = hostlist_next(itr)) != NULL) {
		int i, proc0;
		
		node = qsw_host2elanid(host);
		for (i = 0; i < procs_per_node; i++) {
			proc0 = (node - cap->LowNode) * procs_per_node;
			if (proc0 + i >= (sizeof(cap->Bitmap) * 8))  {
				printf("Bit %d too big for %d byte bitmap\n",
					proc0 + i, sizeof(cap->Bitmap));
				return -1;
			}
			BT_SET(cap->Bitmap, proc0 + i);
		}
	}
	hostlist_iterator_destroy(itr);

	return 0;
}

/*
 * Set a variable in the callers environment.  Args are printf style.
 * XXX Space is allocated on the heap and will never be reclaimed.
 * Example: setenvf("RMS_RANK=%d", rank);
 */
static int
setenvf(const char *fmt, ...) 
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

static int
qsw_rms_setenv(qsw_info_t *qi)
{
	/* MPI wants some of these ... */
	if (setenvf("RMS_RANK=%d", qi->rank) < 0)
		return -1;
	if (setenvf("RMS_NODEID=%d", qi->nodeid) < 0)
		return -1;
	if (setenvf("RMS_PROCID=%d", qi->procid) < 0)
		return -1;
	if (setenvf("RMS_NNODES=%d", qi->nnodes) < 0)
		return -1;
	if (setenvf("RMS_NPROCS=%d", qi->nprocs) < 0)
		return -1;

	/* XXX these are probably unnecessary */
	if (setenvf("RMS_MACHINE=yourmom") < 0)
		return -1;
	if (setenvf("RMS_RESOURCEID=pdsh.%d", qi->prgnum) < 0)
		return -1;
	if (setenvf("RMS_JOBID=%d", qi->prgnum) < 0)
		return -1;
	return 0;
}

/*
 * capability -> string
 */
int
qsw_encode_cap(char *s, int len, ELAN_CAPABILITY *cap)
{
	assert(sizeof(cap->UserKey.Values[0]) == 4);
	assert(sizeof(cap->UserKey) / sizeof(cap->UserKey.Values[0]) == 4);

	snprintf(s, len, "%x.%x.%x.%x.%hx.%hx.%x.%x.%x.%x.%x.%x.%x:", 
				cap->UserKey.Values[0],
				cap->UserKey.Values[1],
				cap->UserKey.Values[2],
				cap->UserKey.Values[3],
				cap->Type,	/* short */
				0,		/* short */
				cap->LowContext,
				cap->HighContext,
				cap->MyContext,
				cap->LowNode,
				cap->HighNode,
				cap->Entries,
				cap->RailMask);

	assert(sizeof(cap->Bitmap[0]) == 4);
	assert(sizeof(cap->Bitmap) / sizeof(cap->Bitmap[0]) == 16); 

	len -= strlen(s);
	s += strlen(s);

	snprintf(s, len, "%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x",
				cap->Bitmap[0], cap->Bitmap[1], 
				cap->Bitmap[2], cap->Bitmap[3],
				cap->Bitmap[4], cap->Bitmap[5], 
				cap->Bitmap[6], cap->Bitmap[7],
				cap->Bitmap[8], cap->Bitmap[9], 
				cap->Bitmap[10], cap->Bitmap[11],
				cap->Bitmap[12], cap->Bitmap[13],
				cap->Bitmap[14], cap->Bitmap[15]);


	return 0;
}

/*
 * string -> capability
 */
int
qsw_decode_cap(char *s, ELAN_CAPABILITY *cap)
{
	short dummy;

	/* initialize capability - not sure if this is necessary */
	elan3_nullcap(cap);

	/* fill in values sent from remote */
	if (sscanf(s, "%x.%x.%x.%x.%hx.%hx.%x.%x.%x.%x.%x.%x.%x:", 
				&cap->UserKey.Values[0],
				&cap->UserKey.Values[1],
				&cap->UserKey.Values[2],
				&cap->UserKey.Values[3],
				&cap->Type,	/* short */
				&dummy,		/* short */
				&cap->LowContext,
				&cap->HighContext,
				&cap->MyContext,
				&cap->LowNode,
				&cap->HighNode,
				&cap->Entries,
				&cap->RailMask) != 13) {
		return -1;
	}

	if ((s = strchr(s, ':')) == NULL)
		return -1;

	if (sscanf(s, ":%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x",
				&cap->Bitmap[0], &cap->Bitmap[1], 
				&cap->Bitmap[2], &cap->Bitmap[3],
				&cap->Bitmap[4], &cap->Bitmap[5], 
				&cap->Bitmap[6], &cap->Bitmap[7],
				&cap->Bitmap[8], &cap->Bitmap[9], 
				&cap->Bitmap[10], &cap->Bitmap[11],
				&cap->Bitmap[12], &cap->Bitmap[13],
				&cap->Bitmap[14], &cap->Bitmap[15]) != 16) {
		return -1;
	}

	return 0;
}

/*
 * string -> info
 */
int
qsw_decode_info(char *s, qsw_info_t *qi)
{
	if (sscanf(s, "%x.%x.%x.%x.%x.%x", 
			&qi->prgnum,
			&qi->rank,
			&qi->nodeid,
			&qi->procid,
			&qi->nnodes,
			&qi->nprocs) != 6) {
		return -1;
	}
	return 0;
}

/*
 * info -> string
 */
int
qsw_encode_info(char *s, int len, qsw_info_t *qi)
{
	snprintf(s, len, "%x.%x.%x.%x.%x.%x",
			qi->prgnum,
			qi->rank,
			qi->nodeid,
			qi->procid,
			qi->nnodes,
			qi->nprocs);
	return 0;
}

/*
 * Generate a random program number.  Normally these would be allocated,
 * but since we have no persistant daemon, we settle for random.
 * Must be called after qsw_init_capability (we seed lrand48 there).
 */
int
qsw_get_prgnum(void)
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
qsw_init_capability(ELAN_CAPABILITY *cap, int nprocs, hostlist_t nodelist,
		int cyclic_alloc)
{
	int i;
	int num_nodes = list_length(nodelist);
	int procs_per_node = nprocs / num_nodes;
	

	srand48(getpid());

	/*
	 * Initialize for multi rail and either block or cyclic allocation.  
	 * Set ELAN_CAP_TYPE_BROADCASTABLE later if appropriate.
	 */
	elan3_nullcap(cap);
	if (cyclic_alloc)
		cap->Type = ELAN_CAP_TYPE_CYCLIC;
	else
		cap->Type = ELAN_CAP_TYPE_BLOCK;
	cap->Type |= ELAN_CAP_TYPE_MULTI_RAIL;
	cap->RailMask = 1;

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
		(ELAN_USER_TOP_CONTEXT_NUM - ELAN_USER_BASE_CONTEXT_NUM + 1);
	cap->LowContext +=  ELAN_USER_BASE_CONTEXT_NUM;
	cap->HighContext = cap->LowContext + procs_per_node - 1;
	/* not necessary to initialize cap->MyContext */

	/*
	 * Describe the mapping of processes to nodes.
	 * This sets cap->HighNode, cap->LowNode, and cap->Bitmap.
	 */
	if (qsw_setbitmap(nodelist, procs_per_node, cap) < 0) {
		err("%p: do all target nodes have an Elan adapter?\n");
		return -1;
	}

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

	/* 
	 * As we now support segmented broadcast, always flag the capability
	 * as broadcastable. 
	 */
	/*if (abs(cap->HighNode - cap->LowNode) == num_nodes - 1)*/
	cap->Type |= ELAN_CAP_TYPE_BROADCASTABLE;

	return 0;
}

/*
 * Take necessary steps to set up to run an Elan MPI "program" 
 * (set of processes) on a node.  
 *
 * Process 1	Process 2	|	Process 3	Process 4
 * read args			|
 * fork	-------	rms_prgcreate	|
 * waitpid 	elan3_create	|
 * 		rms_prgaddcap	|
 *		fork N procs ---+------	rms_setcap
 *		wait all	|	setup RMS_ env	
 *				|	fork ----------	setuid, etc.
 *				|	wait		exec mpi process
 *				|	exit
 *		exit		|
 * rms_prgdestroy		|
 * exit				|     (one pair of processes per mpi proc!)
 *
 * Excessive forking seems to be required!  
 * - The first fork is required because rms_prgdestroy can't occur in the 
 *   process that calls rms_prgcreate (since it is a member, ECHILD).
 * - The second fork is required when running multiple processes per node 
 *   because each process must announce its use of one of the hw contexts 
 *   in the range allocated in the capability.
 * - The third fork seems required after the rms_setcap or else elan3_attach
 *   will fail with EINVAL.
 *
 * One process:
 *    init-xinetd-+-in.qshd---in.qshd---in.qshd---in.qshd---sleep
 * Two processes:
 *    init-xinetd-+-in.qshd---in.qshd---2*[in.qshd---in.qshd---sleep]
 * (if stderr backchannel is active, add one in.qshd)
 *   
 * Any errors result in a message on stderr and program exit.
 */
void
qsw_setup_program(ELAN_CAPABILITY *cap, qsw_info_t *qi, uid_t uid)
{
	int pid; 
	int cpid[ELAN_MAX_VPS];
	int procs_per_node; 
	int proc_index;

	if (qi->nprocs > ELAN_MAX_VPS) /* should catch this in client */
		errx("%p: too many processes requested\n");

	/* 
	 * First fork.  Parent waits for child to terminate, then cleans up.
	 */
	pid = fork();
	switch (pid) {
		case -1:	/* error */
			errx("%p: fork: %m\n");
		case 0:		/* child falls thru */
			break;
		default:	/* parent */
			if (waitpid(pid, NULL, 0) < 0)
				errx("%p: waitpid: %m\n");
			while (rms_prgdestroy(qi->prgnum) < 0) {
				if (errno != ECHILD)
					errx("%p: rms_prgdestroy: %m\n");
				sleep(1); /* waitprg would be nice! */
			}
			exit(0);
	}
	/* child continues here */

	/* 
	 * Set up capability 
	 */
	{
		int i, nrails;

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
			if ((ctx = elan3_control_open (i)) == NULL)
				errx("%p: elan3_control_open(%d): %m\n", i);

			/* Push capability into device driver */
			if (elan3_create(ctx, cap) < 0)
				errx("%p: elan3_create failed: %m\n");                      
		}
	}                                            

	/* associate this process and its children with prgnum */
	if (rms_prgcreate(qi->prgnum, uid, 1) < 0)	/* 1 cpu (bogus!) */
		errx("%p: rms_prgcreate %d failed: %m\n", qi->prgnum);

	/* make cap known via rms_getcap/rms_ncaps to members of this prgnum */
	if (rms_prgaddcap(qi->prgnum, 0, cap) < 0)
		errx("%p: rms_prgaddcap failed: %m\n");

	if (debug_syslog) {
		char tmpstr[1024];

		syslog(LOG_DEBUG, "prg %d cap %s bitmap 0x%.8x", qi->prgnum,
			elan3_capability_string(cap, tmpstr), cap->Bitmap[0]);
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
			pid = waitpid(0, NULL, 0); /* any in pgrp */
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
	if (qsw_rms_setenv(qi) < 0)
		errx("%p: failed to set environment variables: %m\n");

	/*
	 * Third fork.  XXX Necessary but I don't know why.
	 */
	pid = fork();
	switch (pid) {
		case -1:	/* error */
			errx("%p: fork: %m\n");
		case 0:		/* child falls thru */
			break;
		default:	/* parent */
			if (waitpid(pid, NULL, 0) < 0)
				errx("%p: waitpid: %m\n");
			exit(0);
	}
	/* child continues here */

	/* Exec the process... */
}

#ifdef TEST_MAIN
/* encode info, then decode and check that the result is what we started with */
static void
verify_info_encoding(qsw_info_t *qi)
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
static void
verify_cap_encoding(ELAN_CAPABILITY *cap)
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
static void 
strncatargs(char *buf, int len, int argc, char *argv[])
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

static void
usage(void)
{
	errx("Usage %p [ -n procs ] [ -u uid ] command args...\n");
}

/* 
 * Test program for qsw runtime routines.  Run one or more processes locally, 
 * e.g. for MPI ping test across shared memory:
 *    qrun -n 2 -u 5588 mping 1 32768
 */
int
main(int argc, char *argv[])
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
		nnodes: 1,
		nprocs: 1,
	};

	err_init(xbasename(argv[0]));	/* init err package */

	while ((c = getopt(argc, argv, "u:n:")) != EOF) {
		switch (c) {
			case 'u':
				uid = atoi(optarg);
				break;
			case 'n':
				qinfo.nprocs = atoi(optarg);
				break;
			default:
				usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0)
		usage();

	/* prep arg for the shell */
	strncatargs(cmdbuf, sizeof(cmdbuf), argc, argv);

	/* create working collective containing only this host */
	if (gethostname(hostname, sizeof(hostname)) < 0)
		errx("%p: gethostname: %m\n");
	if ((p = strchr(hostname, '.')))
		*p = '\0';
	hostlist_push(wcoll, hostname);

	/* initialize capability for this "program" */
	if (qsw_init_capability(&cap, qinfo.nprocs/qinfo.nnodes, wcoll, 0) < 0)
		errx("%p: failed to initialize Elan capability\n");

	/* assert encode/decode routines work (we don't use them here) */
	verify_info_encoding(&qinfo);
	verify_cap_encoding(&cap);

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

	exit(0);
}
#endif /* TEST_MAIN */
#endif /* HAVE_ELAN */

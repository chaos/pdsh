/* 
 * $Id$
 *
 * Copyright (C) 2001 Regents of the University of California
 * See ./DISCLAIMER
 */

#include "conf.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <paths.h>
#include <stdarg.h>
#include <ctype.h>
#include <assert.h>
#include <stdlib.h>

#include <elan3/elan3.h>
#include <elan3/elanvp.h>
#include <rms/rmscall.h>

#include "list.h"
#include "qswutil.h"
#include "err.h"

/* we will allocate program descriptions in this range */
/* XXX note: do not start at zero as libelan shifts to get unique shm id */
#define QSW_PRG_START  1
#define QSW_PRG_END    100

/* we will allocate hardware context numbers in this range */
#define QSW_HWCX_START	ELAN_USER_BASE_CONTEXT_NUM
#define QSW_HWCX_END	ELAN_USER_TOP_CONTEXT_NUM


/* 
 * Convert hostname to elan node number.  This version just returns
 * the numerical part of the hostname, true on our current systems.
 * Other methods such as a config file or genders attribute might be 
 * appropriate for wider uses.
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
 * Given a list of hostnames and the number of tasks per node, 
 * set the correct bits in the capability's bitmap and set high and
 * low node id's.
 */
static int
qsw_setbitmap(list_t nodelist, int tasks_per_node, ELAN_CAPABILITY *cap)
{
	int i, j, task0, node;

	/* determine high and low node numbers */
	cap->HighNode = cap->LowNode = -1;
	for (i = 0; i < list_length(nodelist); i++) {
		node = qsw_host2elanid(list_nth(nodelist, i));
		if (node < 0)
			return -1;
		if (node < cap->LowNode || cap->LowNode == -1)
			cap->LowNode = node;
		if (node > cap->HighNode || cap->HighNode == -1)
			cap->HighNode = node;
	}
	if (cap->HighNode == -1 || cap->LowNode == -1)
		return -1;

	/*
	 * The bits represent a task slot between LowNode and HighNode.
	 * If there are N tasks per node, there are N bits per node in the map.
	 * For example, if nodes 4 and 6 are running two tasks per node,
	 * bits 0,1 (corresponding to the two tasks on node 4) and bits 4,5
	 * (corresponding to the two tasks running no node 6) are set.
	 */
	for (i = 0; i < list_length(nodelist); i++) {
		node = qsw_host2elanid(list_nth(nodelist, i));
		for (j = 0; j < tasks_per_node; j++) {
			task0 = (node - cap->LowNode) * tasks_per_node;
			if (task0 + j >= (sizeof(cap->Bitmap) * 8))  {
				printf("Bit %d too big for %d byte bitmap\n",
					task0 + j, sizeof(cap->Bitmap));
				return -1;
			}
			BT_SET(cap->Bitmap, task0 + j);
		}
	}

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
				cap->Generation,/* short */
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
	/* initialize capability - not sure if this is necessary */
	elan3_nullcap(cap);

	/* fill in values sent from remote */
	if (sscanf(s, "%x.%x.%x.%x.%hx.%hx.%x.%x.%x.%x.%x.%x.%x:", 
				&cap->UserKey.Values[0],
				&cap->UserKey.Values[1],
				&cap->UserKey.Values[2],
				&cap->UserKey.Values[3],
				&cap->Type,	/* short */
				&cap->Generation,/* short */
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

int
qsw_get_prgnum(void)
{
	int prgnum;

	/*
	 * Generate a random program number.  Same comment about lack of 
	 * persistant daemon above applies.
	 */
	prgnum = lrand48() % (QSW_PRG_END - QSW_PRG_START + 1);	
	prgnum += QSW_PRG_START;

	return prgnum;
}

/*
 * Prepare a capability that will be passed to all the tasks in a parallel job.
 * Function returns a progam number to use for the job on success, -1 = fail.
 */
int
qsw_init_capability(ELAN_CAPABILITY *cap, int tasks_per_node, list_t nodelist)
{
	int i;

	srand48(getpid());

	/*
	 * Assuming block as opposed to cyclic task allocation, and
	 * single rail (switch plane).
	 */
	elan3_nullcap(cap);
	cap->Type = ELAN_CAP_TYPE_BLOCK;
	cap->Type |= ELAN_CAP_TYPE_BROADCASTABLE;	/* XXX ever not? */
	cap->Type |= ELAN_CAP_TYPE_MULTI_RAIL;
	cap->RailMask = 1;

	/*
	 * UserKey is 128 bits of randomness which should be kept private.
	 */
        for (i = 0; i < 4; i++)
		cap->UserKey.Values[i] = lrand48();

	/*
	 * Elan hardware context numbers must be unique per node.
 	 * One is allocated to each parallel task.  In order for tasks on the
	 * same node to communicate, they must use contexts in the hi-lo range
	 * of a common capability.  With pdsh we have no persistant daemon
	 * to allocate these, so we settle for a random one.  
	 */
	cap->LowContext = lrand48() % (QSW_HWCX_END - QSW_HWCX_START + 1);
	cap->LowContext += QSW_HWCX_START;
	cap->HighContext = cap->LowContext + tasks_per_node - 1;
	/* not necessary to initialize cap->MyContext */

	/*
	 * Describe the mapping of tasks to nodes.
	 * This sets cap->HighNode, cap->LowNode, and cap->Bitmap.
	 */
	if (qsw_setbitmap(nodelist, tasks_per_node, cap) < 0) {
		err("%p: do all target nodes have an Elan adapter?");
		return -1;
	}
	cap->Entries = list_length(nodelist) * tasks_per_node;

	if (cap->Entries > ELAN_MAX_VPS) {
		err("%p: too many tasks requested (max %d)", ELAN_MAX_VPS);
		return -1;
	}

	return 0;
}

/*
 * Take necessary steps to set up to run an Elan MPI "program" (set of tasks)
 * on a node.  
 *
 * Process 1	Process 2	|	Process 3	Process 4
 * read args			|
 * fork	-------	rms_prgcreate	|
 * waitpid 	elan3_create	|
 * 		rms_prgaddcap	|
 *		fork N tasks ---+------	rms_setcap
 *		wait all	|	setup RMS_ env	
 *				|	fork ----------	setuid, etc.
 *				|	wait		exec mpi task
 *				|	exit
 *		exit		|
 * rms_prgdestroy		|
 * exit				|     (one pair of processes per task!)
 *
 * Excessive forking seems to be required!  
 * - The first fork is required because rms_prgdestroy can't occur in the 
 *   process that calls rms_prgcreate (since it is a member, ECHILD).
 * - The second fork is required when running multiple tasks per node because
 *   each process must announce its use of one of the hw contexts in the range
 *   allocated in the capability.
 * - The third fork seems required after the rms_setcap or else elan3_attach
 *   will fail wiht EINVAL.
 *
 * One task:
 *    init-xinetd-+-in.qshd---in.qshd---in.qshd---in.qshd---sleep
 * Two tasks:
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
	ELAN3_CTX *ctx;
	char tmpstr[1024];
	int tasks_per_node; 
	int task_index;

	if (qi->nprocs > ELAN_MAX_VPS) /* should catch this in client */
		errx("%p: too many tasks requested");

	/* 
	 * First fork.  Parent waits for child to terminate, then cleans up.
	 */
	pid = fork();
	switch (pid) {
		case -1:	/* error */
			errx("%p: fork: %m");
		case 0:		/* child falls thru */
			break;
		default:	/* parent */
			if (waitpid(pid, NULL, 0) < 0)
				errx("%p: waitpid: %m");
			while (rms_prgdestroy(qi->prgnum) < 0) {
				if (errno != ECHILD)
					errx("%p: rms_prgdestroy: %m");
				sleep(1); /* waitprg would be nice! */
			}
			exit(0);
	}
	/* child continues here */

	/* obtain an Elan context to use in call to elan3_create */
	if ((ctx = _elan3_init(0)) == NULL)
		errx("%p: _elan3_init failed: %m");

	/* associate this process and its children with prgnum */
	if (rms_prgdestroy(qi->prgnum) == 0) 
		err("%p: cleaned up old prgnum %d", qi->prgnum);
	if (rms_prgcreate(qi->prgnum, uid, 1) < 0)	/* 1 cpu (bogus!) */
		errx("%p: rms_prgcreate %d failed: %m", qi->prgnum);

	/* make cap known via rms_getcap/rms_ncaps to members of this prgnum */
	if (elan3_create(ctx, cap) < 0)
		errx("%p: elan3_create failed: %m");
	if (rms_prgaddcap(qi->prgnum, 0, cap) < 0)
		errx("%p: rms_prgaddcap failed: %m");

	syslog(LOG_DEBUG, "prg %d cap %s bitmap 0x%.8x", qi->prgnum,
			elan3_capability_string(cap, tmpstr), cap->Bitmap[0]);



	/* 
	 * Second fork - once for each task.
	 * Parent waits for all children to exit the it exits.
	 * Child assigns hardware context to each task, then forks again...
	 */
	tasks_per_node = qi->nprocs / qi->nnodes;
	for (task_index = 0; task_index < tasks_per_node; task_index++) {
		cpid[task_index] = fork();
		if (cpid[task_index] < 0)
			errx("%p: fork (%d): %m", task_index);
		else if (cpid[task_index] == 0)
			break;
	}
	/* parent */
	if (task_index == tasks_per_node) {
		int waiting = tasks_per_node;
		int i;

		while (waiting > 0) {
			pid = waitpid(0, NULL, 0); /* any in pgrp */
			if (pid < 0)
				errx("%p: waitpid: %m");
			for (i = 0; i < tasks_per_node; i++) {
				if (cpid[i] == pid)
					waiting--;
			}
		}
		exit(0);
	}
	/* child falls through here */

	/*
	 * Assign elan hardware context to current process.
	 * - arg1 is an index into the kernel's list of caps for this 
	 *   program desc (added by rms_prgaddcap).  There will be
	 *   one per rail.
	 * - arg2 indexes the hw ctxt range in the capability
	 *   [cap->LowContext, cap->HighContext]
	 */
	if (rms_setcap(0, task_index) < 0)
		errx("%p: rms_setcap (%d): %m", task_index);

	/* set RMS_ environment vars */
	qi->procid = qi->rank = (qi->nodeid * tasks_per_node) + task_index;
	if (qsw_rms_setenv(qi) < 0)
		errx("%p: failed to set environment variables: %m");

	/*
	 * Third fork.  XXX Necessary but I don't know why.
	 */
	pid = fork();
	switch (pid) {
		case -1:	/* error */
			errx("%p: fork: %m");
		case 0:		/* child falls thru */
			break;
		default:	/* parent */
			if (waitpid(pid, NULL, 0) < 0)
				errx("%p: waitpid: %m");
			exit(0);
	}
	/* child continues here */

	/* Exec the task... */
}

#ifdef TEST_MAIN
/* test */
int
main(int argc, char *argv[])
{
	ELAN_CAPABILITY cap;
	ELAN_CAPABILITY cap2;
	char tmpstr[1024];
	char *p;
	int err;
	qsw_info_t qi, qi2;
	list_t wcoll = list_new();

	/* test encoding/decoding/initializing Elan capabilities */
	list_push(wcoll, "foo0");
	list_push(wcoll, "foo1");
	list_push(wcoll, "foo6");
	list_push(wcoll, "foo7");
	err = qsw_init_capability(&cap, 4, wcoll);
	assert(err >= 0);
	printf("%s %x\n", elan3_capability_string(&cap, tmpstr), cap.Bitmap[0]);
	err = qsw_encode_cap(tmpstr, sizeof(tmpstr), &cap);
	assert(err >= 0);
	err = qsw_decode_cap(tmpstr, &cap2);
	assert(err >= 0);
	/* ELAN_CAP_MATCH is broken (see QSW GNATS#3875) */
	/*assert(ELAN_CAP_MATCH(&cap, &cap2)); */ 
	assert(memcmp(&cap, &cap2, sizeof(cap));

	/* test qsw_setenvf function */
	err = qsw_setenvf("SNERG=%d%s", 42, "blah");
	assert(err == 0);
	err = qsw_setenvf("BLURG=%s%d", "sniff", 11);
	assert(err == 0);
	p = getenv("SNERG");
	assert(p != NULL);
	assert(strcmp(p, "42blah") == 0);
	p = getenv("BLURG");
	assert(p != NULL);
	assert(strcmp(p, "sniff11") == 0);

	/* test encoding/decoding qsw_info_t struct */
	qi.prgnum = qsw_get_prgnum();
	qi.rank = 0;
	qi.nodeid = 0;
	qi.procid = 0;
	qi.nnodes = 4;
	qi.nprocs = 16;
	err = qsw_encode_info(tmpstr, sizeof(tmpstr), &qi);
	assert(err >= 0);
	err = qsw_decode_info(tmpstr, &qi2);
	assert(memcmp(&qi, &qi2, sizeof(qi)) == 0);

	printf("All tests passed.\n");
	exit(0);
}
#endif

/* 
 * $Id$
 *
 * Copyright (C) 2001 Regents of the University of California
 * See ./DISCLAIMER
 */

#include "conf.h"

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
#include "base64.h"
#include "qswutil.h"

/* we will allocate program descriptions in this range */
#define ELAN_PRG_START  0
#define ELAN_PRG_END    3

/* we will allocate hardware context numbers in this range */
#define ELAN_HWCX_START	ELAN_USER_BASE_CONTEXT_NUM
#define ELAN_HWCX_END	ELAN_USER_TOP_CONTEXT_NUM


/* 
 * Convert hostname to elan node number.  This version just returns
 * the numerical part of the hostname, true on our current systems.
 * Other methods such as a config file or genders attribute might be 
 * appropriate for wider uses.
 * 	host (IN)		hostname
 *	nodenum (RETURN)	elanid (-1 on failure)
 */
int
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
int
qsw_setenvf(const char *fmt, ...) 
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

int
qsw_qshell_setenv(qsw_info_t *qi)
{
	if (qsw_setenvf("RMS_RANK=%d", qi->rank) < 0)
		return -1;
	if (qsw_setenvf("RMS_NODEID=%d", qi->nodeid) < 0)
		return -1;
	if (qsw_setenvf("RMS_PROCID=%d", qi->procid) < 0)
		return -1;
	if (qsw_setenvf("RMS_NNODES=%d", qi->nnodes) < 0)
		return -1;
	if (qsw_setenvf("RMS_NPROCS=%d", qi->nprocs) < 0)
		return -1;
	return 0;
}

/*
 * capability -> string
 */
int
qsw_pack_cap(char *s, int len, ELAN_CAPABILITY *cap)
{
	int err;

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

	len -= strlen(s);
	s += strlen(s);

	err = encode_base64((unsigned char const *)&cap->Bitmap, 
			sizeof(cap->Bitmap), s, len);
	assert(err >= 0);
	assert(strlen(s) < len);

	return 0;
}

/*
 * string -> capability
 */
int
qsw_unpack_cap(char *s, ELAN_CAPABILITY *cap)
{
	char *p;
	int err;

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

	p = strchr(s, ':');
	if (p == NULL)
		return -1;

	/* XXX bug in decode_base64 requires > original size in target */
	err = decode_base64(p + 1, (unsigned char *)&cap->Bitmap, 
			sizeof(cap->Bitmap) + 4);
	assert(err <= sizeof(cap->Bitmap));
	if (err != sizeof(cap->Bitmap))
		return -1;

	return 0;
}

int
qsw_unpack_info(char *s, qsw_info_t *qi)
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
qsw_pack_info(char *s, int len, qsw_info_t *qi)
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
	prgnum = lrand48() % (ELAN_PRG_END - ELAN_PRG_START + 1);	
	prgnum += ELAN_PRG_START;

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
	cap->LowContext = lrand48() % (ELAN_HWCX_END - ELAN_HWCX_START + 1);
	cap->LowContext += ELAN_HWCX_START;
	cap->HighContext = cap->LowContext + tasks_per_node - 1;
	cap->MyContext = cap->LowContext; /* override per task */

	/*
	 * Describe the mapping of tasks to nodes.
	 * This sets cap->HighNode, cap->LowNode, and cap->Bitmap.
	 */
	if (qsw_setbitmap(nodelist, tasks_per_node, cap) < 0)
		return -1;
	cap->Entries = list_length(nodelist) * tasks_per_node;

	return 0;
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

	/* test packing/unpacking/initializing Elan capabilities */
	list_push(wcoll, "foo0");
	list_push(wcoll, "foo1");
	list_push(wcoll, "foo6");
	list_push(wcoll, "foo7");
	err = qsw_init_capability(&cap, 4, wcoll);
	assert(err >= 0);
	printf("%s %x\n", elan3_capability_string(&cap, tmpstr), cap.Bitmap[0]);
	err = qsw_pack_cap(tmpstr, sizeof(tmpstr), &cap);
	assert(err >= 0);
	err = qsw_unpack_cap(tmpstr, &cap2);
	assert(err >= 0);
	assert(ELAN_CAP_MATCH(&cap, &cap2));

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

	/* test packing/unpacking qsw_info_t struct */
	qi.prgnum = qsw_get_prgnum();
	qi.rank = 0;
	qi.nodeid = 0;
	qi.procid = 0;
	qi.nnodes = 4;
	qi.nprocs = 16;
	err = qsw_pack_info(tmpstr, sizeof(tmpstr), &qi);
	assert(err >= 0);
	err = qsw_unpack_info(tmpstr, &qi2);
	assert(memcmp(&qi, &qi2, sizeof(qi)) == 0);

	printf("All tests passed.\n");
	exit(0);
}
#endif

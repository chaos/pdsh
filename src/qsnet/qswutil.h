/* 
 * $Id$
 *
 * Copyright (C) 2001 Regents of the University of California
 * See ./DISCLAIMER
 */

#ifndef _QSWUTIL_INCLUDED
#define _QSWUTIL_INCLUDED

typedef struct {
	int prgnum;
	int rank;
	int nodeid;
	int procid;
	int nnodes;
	int nprocs;
} qsw_info_t;

int 	qsw_encode_cap(char *s, int len, ELAN_CAPABILITY *cap);
int 	qsw_decode_cap(char *s, ELAN_CAPABILITY *cap);
int 	qsw_encode_info(char *s, int len, qsw_info_t *qi);
int 	qsw_decode_info(char *s, qsw_info_t *qi);
int 	qsw_init_capability(ELAN_CAPABILITY *cap, int procs_per_node, 
		list_t nodelist, int cyclic_alloc);
int	qsw_get_prgnum(void);
void 	qsw_setup_program(ELAN_CAPABILITY *cap, qsw_info_t *qi, uid_t uid);

#endif /* _QSWUTIL_INCLUDED */

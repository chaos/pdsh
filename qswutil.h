/* 
 * $Id$
 *
 * Copyright (C) 2001 Regents of the University of California
 * See ./DISCLAIMER
 */

typedef struct {
	int prgnum;
	int rank;
	int nodeid;
	int procid;
	int nnodes;
	int nprocs;
} qsw_info_t;

int 	qsw_host2elanid(char *host);
int 	qsw_setenvf(const char *fmt, ...);
int 	qsw_qshell_setenv(qsw_info_t *qi);
int 	qsw_pack_cap(char *s, int len, ELAN_CAPABILITY *cap);
int 	qsw_unpack_cap(char *s, ELAN_CAPABILITY *cap);
int 	qsw_pack_info(char *s, int len, qsw_info_t *qi);
int 	qsw_unpack_info(char *s, qsw_info_t *qi);
#ifdef _LIST_INCLUDED
int 	qsw_init_capability(ELAN_CAPABILITY *cap, int tasks_per_node, 
		list_t nodelist);
#endif
int	qsw_get_prgnum(void);

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
 *  For details, see <http://www.llnl.gov/linux/powerman/>.
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

#ifndef _QSWUTIL_INCLUDED
#define _QSWUTIL_INCLUDED

#include "hostlist.h"

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
int 	qsw_init_capability(ELAN_CAPABILITY *cap, int nprocs,
		hostlist_t nodelist, int cyclic_alloc);
int	qsw_get_prgnum(void);
void 	qsw_setup_program(ELAN_CAPABILITY *cap, qsw_info_t *qi, uid_t uid);

#endif /* _QSWUTIL_INCLUDED */

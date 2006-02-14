/*****************************************************************************\
 *  $Id$
 *****************************************************************************
 *  Copyright (C) 2001-2006 The Regents of the University of California.
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

#ifndef _QSWUTIL_INCLUDED
#define _QSWUTIL_INCLUDED

#if defined(HAVE_LIBELANCTRL)
#  include <elan/capability.h>
#elif defined(HAVE_LIBELAN3)
#  include <elan3/elan3.h>
#endif

#include "src/common/hostlist.h"

#ifndef ELAN_MAX_RAILS
#  define ELAN_MAX_RAILS 16
#endif
#define QSW_RAILMASK_MAX    ((1<<ELAN_MAX_RAILS) - 1)

typedef struct {
    int prgnum;
    int rank;
    int nodeid;
    int procid;
    int nnodes;
    int nprocs;
} qsw_info_t;

int qsw_init(void);
void qsw_fini(void);

int qsw_encode_cap(char *s, int len, ELAN_CAPABILITY * cap);
int qsw_encode_cap_bitmap(char *s, int len, ELAN_CAPABILITY * cap, int i);
int qsw_decode_cap(char *s, ELAN_CAPABILITY * cap);
int qsw_decode_cap_bitmap(char *s, ELAN_CAPABILITY * cap, int i);
int qsw_cap_bitmap_count(void);
int qsw_encode_info(char *s, int len, qsw_info_t * qi);
int qsw_decode_info(char *s, qsw_info_t * qi);
int qsw_init_capability(ELAN_CAPABILITY * cap, int nprocs,
                        hostlist_t nodelist, int cyclic_alloc, 
                        unsigned int railmask);
int qsw_get_prgnum(void);
void qsw_setup_program(ELAN_CAPABILITY * cap, qsw_info_t * qi, uid_t uid);
int qsw_prgsignal(int prgid, int signo);

int qsw_spawn_neterr_thr(void);

#endif                          /* _QSWUTIL_INCLUDED */

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

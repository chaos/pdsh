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

#ifndef _WCOLL_INCLUDED
#define _WCOLL_INCLUDED

#if     HAVE_CONFIG_H
#include "config.h"
#endif

#include "list.h"               /* for list_t */

#ifdef HAVE_LIBGENDERS
#include <genders.h>

#ifdef HAVE_LIBNODEUPDOWN
#include <nodeupdown.h>
#endif

#endif

#ifndef _BOOL_DEFINED
#define _BOOL_DEFINED
typedef enum { false, true } bool;
#endif

hostlist_t read_wcoll(char *, FILE *);

#if	HAVE_SDR
hostlist_t sdr_wcoll(bool, bool, bool);
#endif
#if 	HAVE_GENDERS
hostlist_t read_genders(char *attr, int ropt);

#ifdef HAVE_LIBGENDERS
#ifndef GENDERS_ALTNAME_ATTRIBUTE
#define GENDERS_ALTNAME_ATTRIBUTE   "altname"
#endif
#ifdef HAVE_LIBNODEUPDOWN
hostlist_t get_verified_nodes(int iopt);
#endif
#endif

#endif
#if 	HAVE_RMSQUERY
hostlist_t rms_wcoll(void);
#endif

#endif


/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

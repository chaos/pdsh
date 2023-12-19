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

#ifndef _WCOLL_INCLUDED
#define _WCOLL_INCLUDED

#if     HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>              /* FILE *     */

#include "src/common/macros.h"
#include "src/common/hostlist.h"

hostlist_t read_wcoll(char *, FILE *);

/*
 *  Read WCOLL file [file] searching colon-separated path [path].
 *
 *  Supports the use of '#include filename' to include other WCOLL files
 *   that are also in [path].
 */
hostlist_t read_wcoll_path (const char *path, const char *file);

#endif
/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

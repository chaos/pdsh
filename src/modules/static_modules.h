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


/* If we are using static modules, mod.c needs to #include this so it
 * will have access to each module's pdsh_module structure, which
 * holds pointers to all information in the module that is important.
 */ 

#ifndef _STATIC_MODULES_H
#define _STATic_MODULES_H

#if HAVE_CONFIG_H
#  include "config.h"
#endif

#if STATIC_MODULES

/* load extern definitions produced by autoconf */
#include "_static_decls"

/* 
 * Array of all pdsh_module structures we are 
 * compiling
 */
struct pdsh_module *static_mods[] = {
/* load pointers produced by autoconf */
#include "_static_pointers"
    NULL
};

/*
 * Names of all the module structures, for debugging 
 */
char *static_mod_names[] = {
/* load names produced by autoconf */
#include "_static_names"
    NULL
};

#endif /* STATIC_MODULES */

#endif /* _STATIC_MODULES_H */

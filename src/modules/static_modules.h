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

/* 
 * Definitions to module information for all modules
 * we are statically compiling
 */

#if HAVE_LIBGENDERS
extern struct pdsh_module genders_module;
#endif

#if HAVE_KRB4
extern struct pdsh_module k4cmd_module;
#endif

#if HAVE_MACHINES
extern struct pdsh_module machines_module;
#endif

#if HAVE_MRSH
extern struct pdsh_module mcmd_module;
#endif

#if HAVE_MQSH
extern struct pdsh_module mqcmd_module;
#endif

#if HAVE_NODEATTR
extern struct pdsh_module nodeattr_module; 
#endif

#if HAVE_LIBNODEUPDOWN
extern struct pdsh_module nodeupdown_module;
#endif

#if HAVE_ELAN
extern struct pdsh_module qcmd_module;
#endif

#if HAVE_RMSQUERY
extern struct pdsh_module rms_module;
#endif

#if HAVE_SDR
extern struct pdsh_module sdr_module;
#endif

/* sshcmd and xrcmd always built */
extern struct pdsh_module sshcmd_module;
extern struct pdsh_module xrcmd_module;

/* 
 * Array of all pdsh_module structures we are 
 * compiling
 */
struct pdsh_module *static_mods[] = {
#if HAVE_LIBGENDERS
    &genders_module,
#endif
#if HAVE_KRB4
    &k4cmd_module,
#endif
#if HAVE_MACHINES
    &machines_module,
#endif
#if HAVE_MRSH
    &mcmd_module,
#endif
#if HAVE_MQSH
    &mqcmd_module,
#endif
#if HAVE_NODEATTR
    &nodeattr_module,
#endif
#if HAVE_LIBNODEUPDOWN
    &nodeupdown_module,
#endif
#if HAVE_ELAN
    &qcmd_module,
#endif
#if HAVE_RMSQUERY
    &rms_module,
#endif
#if HAVE_SDR
    &sdr_module,
#endif
    &sshcmd_module,
    &xrcmd_module,
    NULL
};

/*
 * Names of all the module structures, for debugging 
 */
char *static_mod_names[] = {
#if HAVE_LIBGENDERS
    "genders",
#endif
#if HAVE_KRB4
    "k4cmd",
#endif
#if HAVE_MACHINES
    "machines",
#endif
#if HAVE_MRSH
    "mcmd",
#endif
#if HAVE_MQSH
    "mqcmd",
#endif
#if HAVE_NODEATTR
    "nodeattr",
#endif
#if HAVE_LIBNODEUPDOWN
    "nodeupdown",
#endif
#if HAVE_ELAN
    "qcmd",
#endif
#if HAVE_RMSQUERY
    "rms",
#endif
#if HAVE_SDR
    "sdr",
#endif
    "sshcmd",
    "xrcmd",
    NULL
};

#endif /* STATIC_MODULES */

#endif /* _STATIC_MODULES_H */

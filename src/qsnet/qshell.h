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

#ifndef _HAVE_QSHELL_H
#define _HAVE_QSHELL_H

#include <netinet/in.h>
#include <pwd.h>

/*
 *  Qshell argument structures
 *   This structure contains the information that is gathered by
 *   the implementation-specific ``getargs'' method passed in to
 *   the qshell() function. 
 *
 *   Optional parameters should be set to NULL if they are not used.
 *
 */
struct qshell_args {
    int               sock;       /* stderr socket                */ 
    int               port;       /* stderror port                */ 
    struct passwd *   pwd;        /* passwd entry for local user  */
    char *            hostname;   /* Remote hostname              */
    char *            remuser;    /* Remote username (optional)   */
    char *            locuser;    /* Local username  (optional)   */
    char *            cmdbuf;     /* Remote command               */
};


/*
 * Function prototype for implementation specific Qshell initialization.
 *   This function is responsible for reading the qshell arguments from
 *   the remote connection, as well as authentication of the remote user.
 *
 *   On success, the `args' qshell_args struct should be filled in
 *   with valid information.
 */
typedef void (*QshGetArgsF)(struct sockaddr_in *, struct qshell_args *args);  


/*
 *  Qshell implementation. 
 *    Initializes qshell connection using the argument count and vector
 *    `ac' and `argv' respectively. The implementation-specific `getargs' 
 *    function is used to read arguments from the remote connection and
 *    authorize and authenticate the user. 
 *
 *    The `name' parameter should be set to  the name of the process 
 *    calling this function (used for error logging). 
 *
 *    If `check_port' is set to true, the connection will be terminated 
 *    if the originating port is not a reserved port.
 *      
 */
int qshell(int ac, char *argv[], QshGetArgsF getargs, char *name, 
           int check_port);


/*  
 *  Error reporting functions 
 *   These may only be called after the stderr connection is complete.
 */
void error(const char *, ...);
void errlog(const char *, ...);

/*
 *  Put a string of maximum length `len' into the destination `dst'
 *   starting at memory location `src.'
 */
int getstr(char *dst, int len, char *src);

/*
 *  Get passwd pointer for a username.
 */
struct passwd *getpwnam_common(char *);

/*
 *  Determine canonical hostname given the sockaddr_in pointer
 */
char *findhostname(struct sockaddr_in *);

#ifdef USE_PAM
int pamauth(struct passwd *, char *, char *, char *, char *);
#endif

#endif  /* !_HAVE_QSHELL_H */

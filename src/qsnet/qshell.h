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

#include <netinet/in.h>
#include <pwd.h>

typedef void (*DoItFunc)(struct sockaddr_in *);  

/* Error reporting functions can only be called after the stderr
 * connection is complete.
 */
void error(const char *, ...);
void errlog(const char *, ...);
void errorsock(int, const char *, ...);

int getstr(char *, int, char *);
struct passwd *getpwnam_common(char *);

char *findhostname(struct sockaddr_in *);
#ifdef USE_PAM
int pamauth(struct passwd *, char *, char *, char *, char *);
#endif

int doit_start(void);              
void doit_end(int, int, struct passwd *pwd, char *, char *, char *, char *);

int main_common(int, char **, DoItFunc, char *, int);

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

/*
 * This code is based on the BSD rcmd.c with MT safety added, and the 
 * interface changed.  Original UC regents header included below.
 */

/*-
 * Copyright (c) 1988, 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * PAM modifications by Michael K. Johnson <johnsonm@redhat.com>
 */

char copyright[] =
"@(#) Copyright (c) 1988, 1989 The Regents of the University of California.\n"
"All rights reserved.\n";

/*
 * From: @(#)rshd.c	5.38 (Berkeley) 3/2/91
 */
char rcsid[] = "$Id$";
/*#include "../version.h"*/

#if     HAVE_CONFIG_H
#include "config.h"
#endif

#if	HAVE_UNISTD_H
#include <unistd.h>          /* rresvport */
#endif
#include <sys/socket.h>      /* connect */
#include <sys/types.h>
#include <netinet/in.h>      /* sockaddr_in, htons */
#include <syslog.h>          /* syslog */
#include <stdio.h>              
#include <stdlib.h>
#include <netdb.h>

#include "src/common/xmalloc.h"
#include "qshell.h"

extern int paranoid;
extern int sent_null;
extern int allow_root_rhosts;
#ifdef USE_PAM
extern char *pam_errmsg;
#endif

static struct passwd *doauth(char *remuser, char *hostname, char *locuser) {
    struct passwd *pwd;

    if ((pwd= getpwnam_common(locuser)) == NULL)
        return NULL;

#ifdef USE_PAM
    if (pamauth(pwd, "qshell", remuser, hostname, locuser) < 0) {
        syslog(LOG_INFO | LOG_AUTH, "PAM Authentication Failure\n");
        error("%s\n", pam_errmsg);
        return NULL;
    }
#else
    if ((pwd->pw_uid == 0 && !allow_root_rhosts) ||
        (ruserok(hostname, pwd->pw_uid == 0, remuser, locuser) < 0)) {
        syslog(LOG_INFO | LOG_AUTH, "Authentication Failure\n");
        error("Permission Denied\n");
        return NULL;
    }
#endif

    return pwd;
}

static void qshd_get_args(struct sockaddr_in *fp, struct qshell_args *args) 
{
    char remuser[16];
    char locuser[16];
    char cmdbuf[ARG_MAX + 1];

    if (args->port != 0) {
        int lport = IPPORT_RESERVED - 1;
        if ((args->sock = rresvport(&lport)) < 0) {
            syslog(LOG_ERR, "can't get stderr port: %m");
            exit(1);
        }
        if (args->port >= IPPORT_RESERVED) {
            syslog(LOG_ERR, "2nd port not reserved\n");
            exit(1);
        }
        fp->sin_port = htons(args->port);
        if (connect(args->sock, (struct sockaddr *) fp, sizeof(*fp)) < 0) {
            syslog(LOG_INFO, "connect second port: %m");
            exit(1);
        }
    }

    /* Get remote user name, local user name, and command */

    if (getstr(remuser, sizeof(remuser), "remuser") < 0)
        exit(1);

    if (getstr(locuser, sizeof(locuser), "locuser") < 0)
        exit(1);

    if (getstr(cmdbuf,  sizeof(cmdbuf),  "command") < 0)
        exit(1);

    if ((args->hostname = findhostname(fp)) == NULL) {
        error("Host Address Mismatch");
        exit(1);
    }

    if ((args->pwd = doauth(remuser, args->hostname, locuser)) == NULL)
        exit(1);

    args->remuser = Strdup(remuser);
    args->locuser = Strdup(remuser);
    args->cmdbuf =  Strdup(cmdbuf);

    return;
}

int main(int argc, char *argv[]) {
    return qshell(argc, argv, &qshd_get_args, "qshd", 1);
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

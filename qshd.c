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

#include "qshd_common.h"

extern int paranoid;
extern int sent_null;
extern int allow_root_rhosts;

static void doit(struct sockaddr_in *fromp);

static struct passwd *doauth(char *remuser, char *hostname, char *locuser) {
    struct passwd *pwd;

    if ((pwd= getpwnam_common(locuser)) == NULL)
        return NULL;

#ifdef USE_PAM
    if (pamauth(pwd, "qshell", remuser, hostname, locuser) < 0)
        return NULL;
#else
    if ((pwd->pw_uid == 0 && !allow_root_rhosts) ||
        (ruserok(hostname, pwd->pw_uid == 0, remuser, locuser) < 0))
        return NULL;
#endif

    return pwd;
}

static void doit(struct sockaddr_in *fromp) {
    char locuser[16], remuser[16], cmdbuf[ARG_MAX + 1];
    int port, sock = -1;
    struct passwd *pwd;
    char *hostname;

    port = doit_start();

    if (port != 0) {
        int lport = IPPORT_RESERVED - 1;
        if ((sock = rresvport(&lport)) < 0) {
            syslog(LOG_ERR, "can't get stderr port: %m");
            exit(1);
        }
        if (port >= IPPORT_RESERVED) {
            syslog(LOG_ERR, "2nd port not reserved\n");
            exit(1);
        }
        fromp->sin_port = htons(port);
        if (connect(sock, (struct sockaddr *) fromp, sizeof(*fromp)) < 0) {
            syslog(LOG_INFO, "connect second port: %m");
            exit(1);
        }
    }

    /* Get remote user name, local user name, and command */

    if (getstr(remuser, sizeof(remuser), "remuser") < 0)
        exit(1);

    if (getstr(locuser, sizeof(locuser), "locuser") < 0)
        exit(1);

    if (getstr(cmdbuf, sizeof(cmdbuf), "command") < 0)
        exit(1);

    if ((hostname = findhostname(fromp)) == NULL)
        exit(1);

    if ((pwd = doauth(remuser, hostname, locuser)) == NULL) {
        syslog(LOG_INFO | LOG_AUTH, "Permission denied\n");
        error("Permission denied");
        exit(1);
    }

    doit_end(sock, port, pwd, remuser, hostname, locuser, cmdbuf);
    exit(1);
}

int main(int argc, char *argv[]) {
    return main_common(argc, argv, &doit, "qshd", 1);
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

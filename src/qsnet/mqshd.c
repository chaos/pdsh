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

/*-
 * Started with BSD rshd which is:
 *
 * Copyright (c) 1988, 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
 *
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * 5. This is free software; you can redistribute it and/or modify it
 *    under the terms of the GNU General Public License as published
 *    by the Free Software Foundation; either version 2 of the
 *    License, or (at your option) any later version.
 *                              
 * 6. This is distributed in the hope that it will be useful, but
 *    WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *                                                           
 * 7. You should have received a copy of the GNU General Public License;
 *    if not, write to the Free Software Foundation, Inc., 59 Temple
 *    Place, Suite 330, Boston, MA  02111-1307  USA.
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
"@(#) Copyright (c) 1988, 1989, 2003 The Regents of the University of California.\n"
"All rights reserved.\n";

/*
 * From: @(#)mqshd.c     5.38 (Berkeley) 3/2/91
 */
char rcsid[] = "$Id$";
/* #include "version.h" */

#if     HAVE_CONFIG_H
#include "config.h"
#endif

#if HAVE_UNISTD_H     
#include <unistd.h>
#endif

#include <fcntl.h>
#include <sys/socket.h>           /* connect */
#include <sys/types.h>
#include <netinet/in.h>           /* sockaddr_in, htons */
#include <arpa/inet.h>            /* inet_ntoa */
#include <netdb.h>                /* gethostbyname */ 
#include <net/if.h>               /* struct ifconf, struct ifreq */
#include <sys/ioctl.h>            /* ioctl */
#include <sys/param.h>            /* MAXHOSTNAMELEN / INET_ADDRSTRLEN */

#include <stdio.h>  
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <munge.h>

#include "src/common/fd.h"
#include "src/common/xmalloc.h"
#include "qshell.h"

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 64
#endif

#ifndef INET_ADDRSTRLEN
#define INET_ADDRSTRLEN 16
#endif

#define MAX_MBUF_SIZE 4096

extern int paranoid;
extern int sent_null;

#define ERRMSGLEN           4096
static char errmsgbuf[ERRMSGLEN];
static const char *errmsg = NULL;

#if USE_PAM
extern char *pam_errmsg;
#endif

static char *munge_parse(char *buf, char *buf_end) {
    int len = strlen(buf);

    buf += (len + 1);
    if (buf >= buf_end) {
        syslog(LOG_ERR, "parser went beyond valid data");
        errmsg = "Internal Error";
        return NULL;
    }
    return buf;
}

static int getifrlen(struct ifreq *ifr) {
    int len;

    /* Calculations below are necessary b/c socket addresses can have
     * variable length 
     */

#if HAVE_SA_LEN
    if (sizeof(struct sockaddr) > ifr->ifr_addr.sa_len)
        len = sizeof(struct sockaddr);
    else
        len = ifr->ifr_addr.sa_len;
#else /* !HAVE_SA_LEN */
    /* For now we only assume AF_INET and AF_INET6 */
    switch(ifr->ifr_addr.sa_family) {
#ifdef HAVE_IPV6
        case AF_INET6:
            len = sizeof(struct sockaddr_in6);
            break;
#endif /* HAVE_IPV6 */
        case AF_INET:
        default:
            len = sizeof(struct sockaddr_in);
            break;
    }

    /* On ia32 struct sockaddr_in6/sockaddr_in was the largest
     * structure in struct ifreq, but not on ia64.  This fixes things
     */
    if (len < (sizeof(struct ifreq) - IFNAMSIZ))
        len = sizeof(struct ifreq) - IFNAMSIZ;

#endif /* HAVE_SA_LEN */ 

    return len;
}

static int check_interfaces(void *munge_addr, int addr_len) {
    struct ifconf ifc;
    struct ifreq *ifr;
    int s, found = 0, lastlen = -1;
    int len = sizeof(struct ifreq) * 100;
    void *buf = NULL, *ptr = NULL;
    struct sockaddr_in *sin;
    char *addr;

    /* Significant amounts of this code are from Unix Network
     * Programming, by R. Stevens, Chapter 16
     */

    if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        syslog(LOG_ERR, "socket call failed: %m");
        errmsg = "Internal System Error";
        goto bad;
    }

    /* get all active interfaces */
    while(1) {
        if ((buf = (char *)malloc(len)) == NULL) {
            syslog(LOG_ERR, "malloc failed: %m");
            errmsg = "Out of Memory";
            goto bad;
        }

        ifc.ifc_len = len;
        ifc.ifc_buf = buf;

        if (ioctl(s, SIOCGIFCONF, &ifc) < 0) {
            syslog(LOG_ERR, "ioctl SIOCGIFCONF failed: %m");
            errmsg = "Internal System Error";
            goto bad;
        }
        else {
            if (ifc.ifc_len == lastlen)
                break;

            lastlen = ifc.ifc_len;
        }

        /* Run ioctl() twice for portability reasons.  See Unix Network
         * Programming, section 16.6
         */

        len += 10 * sizeof(struct ifreq);
        free(buf);
    }

    /* get IP addresses for all interfaces */
    for (ptr = buf; ptr < buf + ifc.ifc_len; ) {

        ifr = (struct ifreq *)ptr;

        len = getifrlen(ifr);

        ptr += sizeof(ifr->ifr_name) + len;

        /* Currently, we only care about IPv4 (i.e. AF_INET) */
        if (ifr->ifr_addr.sa_family != AF_INET)
            continue;

        sin = (struct sockaddr_in *)&ifr->ifr_addr;

        /* Skip 127.0.0.1 */
        addr = inet_ntoa(sin->sin_addr);
        if (strcmp(addr,"127.0.0.1") == 0)
            continue;

        if (memcmp(munge_addr, (void *)&sin->sin_addr.s_addr, addr_len) == 0) {
            found++;
            break;
        }
    }

    free(buf);
    return found;

bad:
    free(buf);
    return -1;
}

static int check_munge_ip(char *ip) {
    struct in_addr in;
    char ipbuf[INET_ADDRSTRLEN];

    strncpy(ipbuf, ip, INET_ADDRSTRLEN);
    ipbuf[INET_ADDRSTRLEN-1] = '\0';

    if (inet_pton(AF_INET, ipbuf, &in) <= 0) {
        syslog(LOG_ERR, "failed inet_pton: %m");
        errmsg = "Internal System Error";
        return -1;
    }
    
    return check_interfaces(&in, sizeof(struct in_addr));
}


static void mqshell_get_args(struct sockaddr_in *fromp, 
                             struct qshell_args *args) 
{
    struct sockaddr_in sin;
    struct passwd cred;
    int rv = -1;
    int buf_length;
    unsigned short port = 0;
    unsigned int randnum;
    char *mptr = NULL;
    char *m_end = NULL;
    char *m_head = NULL;
    char mbuf[MAX_MBUF_SIZE];
    char cmdbuf[ARG_MAX + 1];

    errmsg = NULL;

    args->sock = -1;

    /* read munge blob for authentication */
    memset(&mbuf[0],0,sizeof(mbuf));
    if ((buf_length = fd_null_read_n(0, &mbuf[0], MAX_MBUF_SIZE)) < 0) {
        syslog(LOG_ERR, "%s: %m", "mqshd: bad read error.");
        errmsg = "Internal System Error";
        goto error_out;
    }

    if (buf_length == 0) {
        syslog(LOG_ERR, "mqshd: null munge credential.");
        errmsg = "Protocol Error";
        goto error_out;
    }

    /*
     * The format of our munge buffer is as follows (each a string terminated
     * with a '\0' (null):
     *
     *                                            SIZE            EXAMPLE
     *                                            ==========      =============
     * remote_user_name                           variable        "mhaskell"
     * '\0'
     * dotted_decimal_address_of_this_server      7-15 bytes      "134.9.11.155"
     * '\0'
     * stderr_port_number                         4-8 bytes       "50111"
     * '\0
     * random number                              1-8 bytes       "1f79ca0e"
     * '\0'
     * users_command                              variable        "ls -al"
     * '\0' '\0'
     *
     */
    mptr = &mbuf[0];
    if ((rv = munge_decode(mbuf,0,(void **)&mptr,&buf_length,
                    &cred.pw_uid,&cred.pw_gid)) != EMUNGE_SUCCESS) {

        syslog(LOG_ERR, "%s: %s", "munge_decode error", munge_strerror(rv));
        snprintf(errmsgbuf, ERRMSGLEN, "Authentication Failure: %s",
                 munge_strerror(rv));
        errmsg = &errmsgbuf[0];
        goto error_out;
    }

    if ((mptr == NULL) || (buf_length <= 0)) {
        syslog(LOG_ERR, "Null munge buffer");
        errmsg = "Protocol Error";
        goto error_out;
    }

    m_head = mptr;
    m_end = mptr + buf_length;

    /* Verify User Id */

    if ((args->pwd = getpwnam_common(m_head)) == NULL) {
        syslog(LOG_ERR, "bad getpwnam(): %m");
        errmsg = "Permission Denied";
        goto error_out;
    }

    if ((args->pwd->pw_uid != cred.pw_uid) && cred.pw_uid != 0) {
        syslog(LOG_ERR, "failed credential check: %m");
        errmsg = "Permission Denied";
        goto error_out;
    }

    /* Verify IP address */

    if ((m_head = munge_parse(m_head, m_end)) == NULL)
        goto error_out;
    
    if ((rv = check_munge_ip(m_head)) < 0)
        goto error_out;

    if (rv == 0) {
        syslog(LOG_ERR, "%s: %s", "Munge IP address doesn't match", m_head);
        errmsg = "Permission Denied";
        goto error_out; 
    }

    /* Verify Port */

    if ((m_head = munge_parse(m_head, m_end)) == NULL)
        goto error_out;

    errno = 0;
    port = strtol(m_head, (char **)NULL, 10);
    if (errno != 0) {
        syslog(LOG_ERR, "%s: %s", "Bad port number from client", m_head);
        errmsg = "Internal Error";
        goto error_out;
    }

    if (port != args->port) {
        syslog(LOG_ERR, "%s: %d, %d", "Port mismatch", args->port, port);
        errmsg = "Protocol Error";
        goto error_out;
    }

    /* Get Random Number */

    if ((m_head = munge_parse(m_head, m_end)) == NULL)
        goto error_out;

    errno = 0;
    randnum = strtol(m_head,(char **)NULL,10);
    if (errno != 0) {
        syslog(LOG_ERR, "%s: %d", "mqshd: Bad random number from client.", randnum);
        errmsg = "Internal Error";
        goto error_out;
    }

    if (args->port == 0 && randnum != 0) {
        syslog(LOG_ERR,"protocol error, rand should be 0, %d", randnum);
        errmsg = "Protocol Error";
        goto error_out;
    }

    /* Get Command */

    if ((m_head = munge_parse(m_head, m_end)) == NULL)
        goto error_out;

    if (strlen(m_head) < ARG_MAX) {
        strncpy(&cmdbuf[0], m_head, sizeof(cmdbuf));
        cmdbuf[sizeof(cmdbuf) - 1] = '\0';
    } else {
        syslog(LOG_ERR, "Not enough space for command: %s", m_head);
        errmsg = "Command too long";
        goto error_out;
    }

    free(mptr);
    mptr = NULL;

    if ((args->hostname = findhostname(fromp)) == NULL) {
        errmsg = "Host Address Mismatch";
        goto error_out;
    }

#ifdef USE_PAM
    if (pamauth(args->pwd, "mqshell", args->pwd->pw_name, 
                args->hostname, args->pwd->pw_name       ) < 0) {
        syslog(LOG_ERR, "PAM failed authentication");
        errmsg = pam_errmsg;
        goto error_out;
    }
#endif

error_out:

    /* If desired, setup stderr connection */
    args->sock = 0;
    if (args->port != 0) {
        char c;

        if ((args->sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            syslog(LOG_ERR,"create socket: %m");
            goto bad;
        }
        sin.sin_family = AF_INET;
        sin.sin_port = htons(args->port);
        sin.sin_addr.s_addr = fromp->sin_addr.s_addr;
        if (connect(args->sock, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
            syslog(LOG_ERR,"%s: %m","connect second port: %m");
            goto bad;
        }

        /* Sync with client to avoid race condition */
        rv = read(0,&c,1);
        if (rv != 1 || c != '\0') { 
            syslog (LOG_ERR, "%s", "mqshd: Client not ready.");
            goto bad;
        }
    }

    if (errmsg != NULL) {
        char buf[BUFSIZ], *bp = buf;
        snprintf(bp, sizeof(buf)-1, "%c%s\n", '\01', errmsg);
        fd_write_n(args->sock, buf, strlen(buf));
        goto bad;
    }

    /* Write random number to stderr */
    if (args->port != 0) {
        randnum = htonl(randnum);
        if ((rv = fd_write_n(args->sock, &randnum, sizeof(randnum))) == -1) {
            syslog(LOG_ERR, "%s: %m", "write to stderr port: ");
            error("Write error, %s\n", strerror(errno));
            goto bad;
        }
    }

    args->remuser = NULL;
    args->locuser = NULL;
    args->cmdbuf =  Strdup(cmdbuf);

    return;

bad:
    free(mptr);
    close(args->sock);
    exit(1);
}

int main(int argc, char *argv[]) {
    return qshell(argc, argv, &mqshell_get_args, "mqshd", 0);
}

/*
 * vi: tabstop=4 shiftwidth=4 expandtab
 */

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

#if     HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/select.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <pwd.h>
#include <grp.h>
#include <sys/syslog.h>
#include <resolv.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <paths.h>
#include <stdarg.h>
#include <ctype.h>
#include <assert.h>

#include <elan3/elan3.h>
#include <elan3/elanvp.h>
#include <rms/rmscall.h>

#include "src/common/xstring.h"
#include "src/common/err.h"
#include "src/common/fd.h"
#include "qswutil.h"
#include "qshell.h"

int keepalive = 1;
int check_all = 0;
int paranoid = 0;
int sent_null = 0;
int allow_root_rhosts = 1;

static char typename[BUFSIZ];

#define OPTIONS "ahlLn"

#if defined(__GLIBC__) && (__GLIBC__ >= 2)
#define _check_rhosts_file  __check_rhosts_file
#endif

extern int _check_rhosts_file;

#ifdef USE_PAM
#include <security/pam_appl.h>
#include <security/pam_misc.h>
pam_handle_t *pamh;
#endif

char username[20] = "USER=";
char homedir[64] = "HOME=";
char shell[64] = "SHELL=";
char path[100] = "PATH=";
char *envinit[] = { homedir, shell, path, username, 0 };
extern char **environ;

static void errcommon(int, const char *, va_list, int);
static int  getcommon(char *);
static int  getint(void);
static int  get_qshell_info(ELAN_CAPABILITY *, qsw_info_t *, char *, int);
static void stderr_parent(int, int, int);
static void network_init(int, struct sockaddr_in *, int);

void errcommon(int fd, const char *fmt, va_list ap, int log) {
    char buf[BUFSIZ], *bp = buf;

    if (sent_null == 0) *bp++ = 1;
    vsnprintf(bp, sizeof(buf) - 1, fmt, ap);
    if (log)
        syslog(LOG_ERR, (sent_null == 0) ? buf + 1 : buf);
    fd_write_n(fd, buf, strlen(buf));
}

void error(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    errcommon(2, fmt, ap, 0);
    va_end(ap);
}

void errlog(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    errcommon(2, fmt, ap, 1);
    va_end(ap);
}

void errorsock(int sock, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    errcommon(sock, fmt, ap, 0);
    va_end(ap);
}

int getcommon(char *err) {
    int rv;
    char c;

    if ((rv = read(0, &c, 1)) < 0) {
        errlog("%s: read %s error: %s\n", typename, err, strerror(errno));
        return -1;
    }

    if (rv != 1) {
        errlog("%s: %s read wrong number of bytes: %d\n", typename, err, rv);
        return -1;
    }

    return (int)c;
}

int getstr(char *buf, int cnt, char *err) {
    int rv;

    do {
        if ((rv = getcommon(err)) == -1)
            return -1;
        *buf++ = (char)rv;

        if (--cnt == 0) {
            errlog("%s: %s too long\n", typename, err);
            return -1;
        }
    } while (rv != 0);

    return cnt;
}

int getint(void) {
    int rv, port = 0;

    do {
        if ((rv = getcommon("port")) == -1)
            return -1;

        if (isascii((char)rv) && isdigit((char)rv))
            port = port * 10 + ((char)rv) - '0';

    } while (rv != 0);

    return port;
}

struct passwd *getpwnam_common(char *user) {
    struct passwd *pwd;

    setpwent();
    if ((pwd= getpwnam(user)) == NULL)
        return NULL;
    if (pwd->pw_uid != 0 && !access(_PATH_NOLOGIN, F_OK))
        return NULL;
    if (pwd->pw_uid == 0 || !strcmp(user, "root"))
        paranoid = 1;

    return pwd;
}

char *findhostname(struct sockaddr_in *fromp) {
    struct hostent *hp;
    char *hostname;
    char *addr = (char *)&fromp->sin_addr;
    int inaddrsize = sizeof(struct in_addr);

    if ((hp = gethostbyaddr(addr, inaddrsize, fromp->sin_family)) == NULL)
        hostname = strdup(inet_ntoa(fromp->sin_addr));
    else
        hostname = strdup(hp->h_name);

    if (hostname == NULL) {
        syslog(LOG_ERR, "System out of memory");
        return NULL;
    }

    /*
     * Attempt to confirm the DNS.
     */
#ifdef  RES_DNSRCH
    _res.options &= ~RES_DNSRCH;
#endif
    if ((hp = gethostbyname(hostname)) == NULL) {
        syslog(LOG_INFO | LOG_AUTH, "Couldn't look up address for %s", hostname);
        return NULL;
    }
    while (hp->h_addr_list[0] != NULL) {
        if (!memcmp(hp->h_addr_list[0], addr, sizeof(fromp->sin_addr))) {
            return hostname;
        }
        hp->h_addr_list++;
    }
    syslog(LOG_NOTICE | LOG_INFO | LOG_AUTH, "Host addr %s not listed for host %s", 
            inet_ntoa(fromp->sin_addr), hp->h_name);
    return NULL;
}

#ifdef USE_PAM
int pamauth(struct passwd *pwd, char *service, char *remuser,
        char *hostname, char *locuser) {
    static struct pam_conv conv = { misc_conv, NULL };
    int ret;

    if ((ret = pam_start(service, locuser, &conv, &pamh)) != PAM_SUCCESS) { 
        syslog(LOG_ERR, "pam_start: %s\n", pam_strerror(pamh, ret));
        return -1;
    }
    pam_set_item(pamh, PAM_RUSER, remuser);
    pam_set_item(pamh, PAM_RHOST, hostname);
    pam_set_item(pamh, PAM_TTY, service);

    if ((ret = pam_authenticate(pamh, 0)) == PAM_SUCCESS)
        ret = pam_acct_mgmt(pamh, 0);

    if (ret == PAM_SUCCESS) {
        /* Why do we do this here instead of near setuid()? */
        if (setgid(pwd->pw_gid) != 0) {
            pam_end(pamh, PAM_SYSTEM_ERR);
            return -1;
        }

        if (initgroups(locuser, pwd->pw_gid) != 0) {
            pam_end(pamh, PAM_SYSTEM_ERR);
            return -1;
        }
        ret = pam_setcred(pamh, PAM_ESTABLISH_CRED);
    }

    if (ret == PAM_SUCCESS)
        ret = pam_open_session(pamh, 0);

    if (ret != PAM_SUCCESS) {
        pam_end(pamh, ret);
        return -1;
    }
    return 0;
}
#endif


/*
 *  Read Qshell arguments from remote side (via stdin)
 *
 *  Returns completed Elan capability struct and qsw_info in
 *   `cap' and `qinfo' on success. Returns -1 on failure.
 *
 */
int get_qshell_info(ELAN_CAPABILITY *cap, qsw_info_t *qinfo, 
                    char *cwd, int cwdlen) {
    int i, envcount;
    char envstr[1024];
    char tmpstr[1024];

    /* read cwd of client - will change to cwd on remote side */
    if (getstr(cwd, cwdlen, "cwd") < 0)
        return -1;

    /* read environment of client - will replicate on remote side */
    if (getstr(tmpstr, sizeof(tmpstr), "envcount") < 0)
        return -1;

    envcount = atoi(tmpstr);
    while (envcount-- > 0) {
        if (getstr(envstr, sizeof(envstr), "envstr") < 0)
            return -1;

        /* Following is a mem-leak on some systems, on others it is
         * the proper way to call putenv.
         */
        putenv(strdup(envstr));
    }

    /* read elan capability */
    if (getstr(tmpstr, sizeof(tmpstr), "capability") < 0)
        return -1;

    if (qsw_decode_cap(tmpstr, cap) < 0) {
        errlog("error reading capability: %s", tmpstr);
        return -1;
    }

    for (i = 0; i < qsw_cap_bitmap_count(); i += 16) {
        if (getstr(tmpstr, sizeof(tmpstr), "capability") < 0)
            return -1;

        if (qsw_decode_cap_bitmap(tmpstr, cap, i) < 0) {
            errlog("error reading capability bitmap(%d): %s", i, tmpstr);
            return -1;
        }
    }

    /* read info structure */
    if (getstr(tmpstr, sizeof(tmpstr), "qsw info") < 0)
        return -1;

    if (qsw_decode_info(tmpstr, qinfo) < 0) {
        errlog("error reading qsw info: %s", tmpstr);
        return -1;
    }

    return 0;
}

void stderr_parent(int sock, int pype, int pid) {
    fd_set ready, readfrom;
    char buf[BUFSIZ], sig;
    int one = 1;
    int nfd, cc, guys = 2;

    ioctl(pype, FIONBIO, (char *) &one);
    /* should set s nbio! */

    FD_ZERO(&readfrom);
    FD_SET(sock, &readfrom);
    FD_SET(pype, &readfrom);
    nfd = (pype > sock) ? pype + 1 : sock + 1; 

    while (guys > 0) {
        ready = readfrom;
        if (select(nfd, &ready, NULL, NULL, NULL) < 0) {
            if (errno != EINTR)
                break;
            continue;
        }
        if (FD_ISSET(sock, &ready)) {
            if (read(sock, &sig, 1) <= 0) {
                FD_CLR(sock, &readfrom);
                guys--;
            }
            /* pid is the "program description" if using Elan */
            else
                rms_prgsignal(pid, sig);
        }
        if (FD_ISSET(pype, &ready)) {
            if ((cc = read(pype, buf, sizeof(buf))) <= 0) {
                shutdown(sock, 2);
                FD_CLR(pype, &readfrom);
                guys--;
            } else
                write(sock, buf, cc);
        }
    }

#ifdef USE_PAM
    /* Must close here, b/c other process runs exec() */
    pam_close_session(pamh, 0);
    pam_end(pamh, PAM_SUCCESS);
#endif
    exit(0);
}


static int _qshell_init(void) {
    int port;

    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);

    /* 
     * Remote side sends us a stringified port number to send back
     * stderr on.  If zero, stderr is folded in with stdout.
     */
    alarm(60);
    if ((port = getint()) < 0) {
        syslog(LOG_ERR, "can't read port: %m");
        exit(1);
    }
    alarm(0);

    return port;
}


/*
 *  Execute the Elan MPI job given the qshell args in `args'
 */
static void _qshell_execute(struct qshell_args *args)
{
    int ifd, pid, pv[2];
    ELAN_CAPABILITY cap;
    qsw_info_t qinfo;
    char *theshell, *shellname;
    char cwd[MAXPATHLEN+1];

    if (get_qshell_info(&cap, &qinfo, cwd, sizeof(cwd)) < 0)
        exit(1);

    /* A single \0 written back on the socket indicates success.  Error
     * would be indicated by a nonzero code followed by error string,
     * e.g. through calling error().
     */
    if (write(2, "\0", 1) != 1) {
        errlog("%s: bad write of null to stdout: %s\n", 
                typename, strerror(errno));
        exit(1);
    }
    sent_null = 1;

    /*
     * Fork off a process to send back stderr if requested.
     */
    if (args->port) {
        if (pipe(pv) < 0) {
            error("Can't make pipe.\n");
            exit(1);
        }
        if ((pid = fork()) == -1) {
            error("Can't fork; try again.\n");
            exit(1);
        }
        if (pid) {
            close(0);
            close(1);
            close(2);
            close(pv[1]);
            stderr_parent(args->sock, pv[0], qinfo.prgnum);
            /* NOTREACHED */
        }
        setpgrp();
        close(args->sock);
        close(pv[0]);
        dup2(pv[1], 2);
        close(pv[1]);
    }

    /*
     * Set up quadrics Elan capabilities and program desc.
     * Fork a couple of times in here.
     * On error, send diagnostics to stderr and exit.
     */
    qsw_setup_program(&cap, &qinfo, args->pwd->pw_uid);

    /*
     *  Become the locuser, etc. etc. then exec the shell command.
     */

    /* set the path to the shell */
    theshell = args->pwd->pw_shell;
    if (!theshell || !*theshell)
        theshell = _PATH_BSHELL;

#if BSD > 43
    if (setlogin(args->pwd->pw_name) < 0)
        errlog("setlogin() failed: %s", strerror(errno));
#endif

#ifndef USE_PAM
    /* if PAM, already done */
    if (setgid(args->pwd->pw_gid)) {
        errlog("setgid: %s", strerror(errno));
        exit(1);
    }
    if (initgroups(args->pwd->pw_name, args->pwd->pw_gid)) {
        errlog("initgroups: %s", strerror(errno));
        exit(1);
    }
#endif

    if (setuid(args->pwd->pw_uid)) {
        errlog("setuid: %s", strerror(errno));
        exit(1);
    }

    strncat(homedir, args->pwd->pw_dir, sizeof(homedir) - 6);
    homedir[sizeof(homedir) - 1] = 0;

    strcat(path, _PATH_DEFPATH);

    strncat(shell, theshell, sizeof(shell) - 7);
    shell[sizeof(shell) - 1] = 0;

    strncat(username, args->pwd->pw_name, sizeof(username) - 6);
    username[sizeof(username) - 1] = 0;

    if ((shellname = strrchr(theshell, '/')) == NULL)
        shellname = theshell;
    else
        shellname++;

    if (paranoid) {
        syslog(LOG_INFO | LOG_AUTH, "%s@%s as %s: cmd='%s' cwd='%s'",
                (args->remuser == NULL) ? args->pwd->pw_name : args->remuser, 
                args->hostname, 
                (args->locuser == NULL) ? args->pwd->pw_name : args->locuser, 
                args->cmdbuf, cwd);
    }

    endpwent();

    /* override USER, HOME, SHELL environment vars */
    putenv(username);
    putenv(homedir);
    putenv(shell);

    /* change to client working dir */
    if (chdir(cwd) < 0) {
        errlog("chdir to client working directory: %s", strerror(errno));
        exit(1);
    }

    /*
     * Close all fds, in case libc has left fun stuff like
     * /etc/shadow open.
     */
#define FDS_INUSE 2
    for (ifd = getdtablesize() - 1; ifd > FDS_INUSE; ifd--)
        close(ifd);

    execl(theshell, shellname, "-c", args->cmdbuf, 0);
    errlog("failed to exec shell: %s", strerror(errno));
    exit(1);
}

void network_init(int fd, struct sockaddr_in *fromp, int check_port) {
    struct linger linger;
    socklen_t fromlen;
    int on = 1;
    int port;

    fromlen = sizeof(*fromp);
    if (getpeername(fd, (struct sockaddr *) fromp, &fromlen) < 0) {
        syslog(LOG_ERR, "getpeername: %m");
        _exit(1);
    }
    if (keepalive &&
        setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (char *) &on, sizeof(on)) < 0)
        syslog(LOG_WARNING, "setsockopt (SO_KEEPALIVE): %m");
    linger.l_onoff = 1;
    linger.l_linger = 60;       /* XXX */
    if (setsockopt(fd, SOL_SOCKET, SO_LINGER, 
                   (char *) &linger, sizeof(linger)) < 0)
        syslog(LOG_WARNING, "setsockopt (SO_LINGER): %m");

    if (fromp->sin_family != AF_INET) {
        syslog(LOG_ERR, "malformed \"from\" address (af %d)\n", fromp->sin_family);
        exit(1);
    }

#ifdef IP_OPTIONS
    {
        u_char optbuf[BUFSIZ / 3], *cp;
        char lbuf[BUFSIZ + 1], *lp;
        socklen_t optsize = sizeof(optbuf);
        int ipproto;
        struct protoent *ip;

        if ((ip = getprotobyname("ip")) != NULL)
            ipproto = ip->p_proto;
        else
            ipproto = IPPROTO_IP;
        if (!getsockopt(0, ipproto, IP_OPTIONS, (char *) optbuf, &optsize) && 
                optsize != 0) {
            lp = lbuf;

            /*
             * If these are true, this will not run off the end of lbuf[].
             */
            assert(optsize <= BUFSIZ / 3);
            assert(3 * optsize <= BUFSIZ);
            for (cp = optbuf; optsize > 0; cp++, optsize--, lp += 3)
                snprintf(lp, 4, " %2.2x", *cp);

            syslog(LOG_NOTICE, "Connection received from %s using IP options"
                  " (ignored): %s", inet_ntoa(fromp->sin_addr), lbuf);

            if (setsockopt(0, ipproto, IP_OPTIONS, NULL, optsize) != 0) {
                syslog(LOG_ERR, "setsockopt IP_OPTIONS NULL: %m");
                exit(1);
            }
        }
    }
#endif

    /*
     * Check originating port for validity.
     */
    if (check_port) {
        port = ntohs(fromp->sin_port);
        if (port >= IPPORT_RESERVED || port < IPPORT_RESERVED / 2) {
            syslog(LOG_NOTICE | LOG_AUTH, "Connection from %s on illegal port",
                    inet_ntoa(fromp->sin_addr));
            exit(1);
        }
    }
}


int qshell(int argc, char *argv[], QshGetArgsF getargs, char *name, 
           int check_port) {
    int ch;
    struct sockaddr_in from;
    struct qshell_args args;
    _check_rhosts_file=1;

    strncpy(typename, name, BUFSIZ);
    openlog(typename, LOG_PID | LOG_ODELAY, LOG_DAEMON);
    err_init(xbasename(argv[0]));

    opterr = 0;
    while ((ch = getopt(argc, argv, OPTIONS)) != EOF) {
        switch (ch) {
            case 'a':
                check_all = 1;
                break;

            case 'h':
                allow_root_rhosts = 1;
                break;

            case 'l':
                _check_rhosts_file = 0;
                break;

            case 'n':
                keepalive = 0;
                break;

            case 'L':
                paranoid = 1;
                break;

            case '?':
            default:
                syslog(LOG_ERR, "usage: %s [-%s]", typename, OPTIONS);
                exit(2);
        }
    }
    argc -= optind;
    argv += optind;

    network_init(0, &from, check_port);

    if (qsw_init() < 0) {
        syslog(LOG_ERR, "qsw_init failed. Exiting...");
        exit(1);
    }

    qsw_spawn_neterr_thr();

    args.port = _qshell_init();
    getargs(&from, &args);
    _qshell_execute(&args);

    qsw_fini();

    return 0;
}

/*
 * vi: tabstop=4 shiftwidth=4 expandtab
 */

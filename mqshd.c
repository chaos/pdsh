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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>

#include <pwd.h>
#include <grp.h>
#include <syslog.h>
#include <resolv.h>
#include <unistd.h>

#include <errno.h>
#include <stdio.h>  /* for vsnprintf */
#include <stdlib.h>
#include <string.h>
#include <paths.h>
#include <stdarg.h>
#include <ctype.h>
#include <assert.h>
#include <net/if.h>

#include <elan3/elan3.h>
#include <elan3/elanvp.h>
#include <rms/rmscall.h>

#include <munge.h>

#include "fd.h"
#include "xstring.h"
#include "err.h"
#include "qswutil.h"

#define HOSTNAME_MAX_LEN 80
#define MAX_MBUF_SIZE 4096

#if defined(__GLIBC__) && (__GLIBC__ >= 2)
#define _check_rhosts_file  __check_rhosts_file
#endif

#ifdef USE_PAM
#include <security/pam_appl.h>
#include <security/pam_misc.h>
static pam_handle_t *pamh;
#endif /* USE_PAM */

#define OPTIONS "ahlLn"

static int keepalive = 1;
static int check_all = 0;
static int paranoid = 0;
static int sent_null;
static int allow_root_rhosts = 1;

struct if_ipaddr_list {
  unsigned int ipaddr;
  struct if_ipaddr_list *next;
};

struct if_ipaddr_list *list_head;

/* error codes */
enum stderr_err {__NONE = 0, __READ, __MUNGE, __PAYLOAD, 
                 __PORT, __CRED, __AUTH, __SYSTEM, __INTERNAL};

enum stderr_err errnum = __NONE;

char username[20] = "USER=";
char homedir[64] = "HOME=";
char shell[64] = "SHELL=";
char path[100] = "PATH=";
char *envinit[] = {homedir, shell, path, username, 0};
extern  char **environ;

static void free_list(struct if_ipaddr_list *list);

static void error(const char *fmt, ...);
static void doit(struct sockaddr_in *fromp);

extern int _check_rhosts_file;

/*
 * Report error to client via socket.
 */
static void
errorsock(int sock, const char *fmt, ...) {
  va_list ap;
  char buf[BUFSIZ], *bp = buf;
    
  if (sent_null == 0) *bp++ = 1;
  va_start(ap, fmt);
  vsnprintf(bp, sizeof(buf)-1, fmt, ap);
  va_end(ap);
  fd_write_n(sock, buf, strlen(buf));
}

/*
 * Report error to client.
 * Note: can't be used until second socket has connected
 * to client, or older clients will hang waiting
 * for that connection first.
 */
static void
error(const char *fmt, ...) {
  va_list ap;
  char buf[BUFSIZ], *bp = buf;
    
  if (sent_null == 0) *bp++ = 1;
  va_start(ap, fmt);
  vsnprintf(bp, sizeof(buf)-1, fmt, ap);
  va_end(ap);
  write(2, buf, strlen(buf));
}

/* same as above but also syslog the error (jg) */
static void
errlog(const char *fmt, ...) {
  va_list ap;
  char buf[BUFSIZ], *bp = buf;
    
  if (sent_null == 0) *bp++ = 1;
  va_start(ap, fmt);
  vsnprintf(bp, sizeof(buf)-1, fmt, ap);
  va_end(ap);
  syslog(LOG_ERR, bp);
  write(2, buf, strlen(buf));
}

static const char *findhostname(struct sockaddr_in *fromp)
{
  struct hostent *hp;
  const char *hostname;

  hp = gethostbyaddr((char *) &fromp->sin_addr, sizeof(struct in_addr),
                     fromp->sin_family);

  errno = ENOMEM;             /* malloc (thus strdup) may not set it */
  if (hp)
    hostname = strdup(hp->h_name);
  else
    hostname = strdup(inet_ntoa(fromp->sin_addr));

  if (hostname == NULL) {
    syslog(LOG_INFO, "out of memory");
    errnum = __SYSTEM;
    return NULL;
  }

  /*
   * Attempt to confirm the DNS.
   */
#ifdef  RES_DNSRCH
  _res.options &= ~RES_DNSRCH;
#endif
  hp = gethostbyname(hostname);
  if (hp == NULL) {
    syslog(LOG_INFO, "Couldn't look up address for %s", hostname);
    errnum = __AUTH;
    return NULL;
  }
  while (hp->h_addr_list[0] != NULL) {
    if (!memcmp(hp->h_addr_list[0], &fromp->sin_addr,
                sizeof(fromp->sin_addr))) {
      return hostname;
    }
    hp->h_addr_list++;
  }
  syslog(LOG_NOTICE, "Host addr %s not listed for host %s",
         inet_ntoa(fromp->sin_addr), hp->h_name);
  errnum = __AUTH;
  return NULL;
}

static int getstr(char *buf, int cnt, const char *err)
{
  int rv;
  char c;

  do {
    if ((rv = read(0, &c, 1)) < 0) { 
      errlog("mqshd: read %s error: %s\n", err, strerror(errno));
      return -1;
    }
    *buf++ = c;
    
    if (rv != 1) {
      errlog("mqshd: %s read wrong number of bytes: %d\n", err, rv);
      return -1;
    }

    if (--cnt == 0) {
      errlog("mqshd: %s too long\n", err);
      return -1;
    }
  } while (c != 0);
  
  return cnt;
}

int get_interface_addresses()
{
  struct ifconf ifc;
  struct ifreq *ifr;
  struct ifreq ifaddr;
  int s, lastlen = -1;
  int len = sizeof(struct ifreq) * 100;
  void *buf = NULL, *ptr = NULL;
  struct if_ipaddr_list *list_node = NULL, *list_tail = NULL;
  struct sockaddr_in * sin;

  list_head = NULL;

  /* A lot of this ioctl code is from Unix Network Programming, by
   * R. Stevens, Chapter 16
   */

  if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    syslog(LOG_ERR, "socket call failed: %m");
    errnum = __SYSTEM;
    goto bad;
  }

  /* get all active interfaces */
  while(1) {
    if ((buf = (char *)malloc(len)) == NULL) {
      syslog(LOG_ERR, "malloc failed: %m");
      errnum = __SYSTEM;
      goto bad;
    }
    
    ifc.ifc_len = len;
    ifc.ifc_buf = buf;
    
    if (ioctl(s, SIOCGIFCONF, &ifc) < 0) {
      syslog(LOG_ERR, "ioctl SIOCGIFCONF failed: %m");
      errnum = __SYSTEM;
      goto bad;
    }
    else {
      if (ifc.ifc_len == lastlen)
        break;

      lastlen = ifc.ifc_len;
    }
    
    /* It looks dumb to run ioctl() twice, but is necessary for
     * portability.  See Unix Network Programming, section 16.6
     */

    len += 10 * sizeof(struct ifreq);
    free(buf);
  }

  /* get IP addresses for all interfaces */
  for (ptr = buf; ptr < buf + ifc.ifc_len; ) {

    ifr = (struct ifreq *)ptr;

    /* Calculate address of next ifreq structure.  Calculations below
     * are necessary b/c socket addresses can have variable length
     */
    
#if HAVE_SA_LEN
    if (sizeof(struct sockaddr) > ifr->ifr_addr.sa_len)
      len = sizeof(struct sockaddr);
    else
      len = ifr->ifr_addr.sa_len;
#else /* !HAVE_SA_LEN */
    /* For now we only assume AF_INET and AF_INET6, we'll 
     * add others as necessary
     */
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
#endif /* HAVE_SA_LEN */ 

    ptr += sizeof(ifr->ifr_name) + len;

    /* Pdsh currently based on IPv4, so we only care about
     * AF_INET network interfaces
     */
    if (ifr->ifr_addr.sa_family != AF_INET)
      continue;

    strcpy(ifaddr.ifr_name, ifr->ifr_name);
    ifaddr.ifr_addr.sa_family = AF_INET;
    if (ioctl(s, SIOCGIFADDR, &ifaddr) < 0) {
      syslog(LOG_ERR, "ioctl SIOCGIFCONF failed: %m");
      errnum = __SYSTEM;
      goto bad;
    }

    /* store address in list */

    if ((list_node = malloc(sizeof(struct if_ipaddr_list))) < 0) {
      syslog(LOG_ERR, "malloc failed: %m");
      errnum = __SYSTEM;
      goto bad;
    }

    if (list_head == NULL) {
      list_head = list_node;
      list_tail = list_node;
    }
    else {
      list_tail->next = list_node;
      list_tail = list_node;
    }

    sin = (struct sockaddr_in *)&ifr->ifr_addr;
    list_node->ipaddr = sin->sin_addr.s_addr;
    list_node->next = NULL;
  }
   
  return 0;

 bad:
  free(buf);
  free_list(list_head);
  return -1;
}

void free_list(struct if_ipaddr_list *list)
{
  struct if_ipaddr_list *temp;
  
  while (list != NULL) {
    temp = list->next;
    free(list);
    list = temp;
  }
}

static void stderr_parent(int sock, int pype, int pid) {
  fd_set ready, readfrom;
  char buf[BUFSIZ], sig;
  int one = 1;
  int nfd, cc, guys=2;
    
  ioctl(pype, FIONBIO, (char *)&one);
  /* should set s nbio! */
    
  FD_ZERO(&readfrom);
  FD_SET(sock, &readfrom);
  FD_SET(pype, &readfrom);
  if (pype > sock) nfd = pype+1;
  else nfd = sock+1;
    
  while (guys > 0) {
    ready = readfrom;
    if (select(nfd, &ready, NULL, NULL, NULL) < 0) {
      if (errno != EINTR) {
        break;
      }
      continue;
    }
    if (FD_ISSET(sock, &ready)) {
      cc = read(sock, &sig, 1);
      if (cc <= 0) {
        FD_CLR(sock, &readfrom);
        guys--;
      }
      /* pid is the "program description" if using Elan */
      else 
        rms_prgsignal(pid, sig);
    }
    if (FD_ISSET(pype, &ready)) {
      cc = read(pype, buf, sizeof(buf));
      if (cc <= 0) {
        shutdown(sock, 2);
        FD_CLR(pype, &readfrom);
        guys--;
      } 
      else write(sock, buf, cc);
    }
  }

#ifdef USE_PAM
  /*
   * This does not strike me as the right place for this; this is
   * in a child process... what does this need to accomplish?
   *
   * No, it's not the child process, the code is just confusing.
   */
  pam_close_session(pamh, 0);
  pam_end(pamh, PAM_SUCCESS);
#endif
    
  exit(0);
}

static void
doit(struct sockaddr_in *fromp)
{
#ifdef USE_PAM
  static struct pam_conv conv = { misc_conv, NULL };
  int retcode;
#endif
  struct sockaddr_in sin;
  struct hostent *m_hostent;
  struct in_addr *h_ptr;
  struct passwd *pwd = NULL;
  struct passwd cred;
  int sock = -1;
  int pv[2], pid, ifd;
  int rv, m_rv = -1;
  int m_arg_char_ctr = 0;
  int buf_length;
  int found;
  int i;
  unsigned int chrnum = 0;
  unsigned int rand;
  unsigned short port = 0;
  unsigned short cport = 0;
  const char *theshell, *shellname;
  const char *hostname = NULL;
  char cmdbuf[ARG_MAX+1];
  char locuser[16], remuser[16];
  char cwd[MAXPATHLEN+1];
  char mbuf[MAX_MBUF_SIZE];
  char *mptr = NULL;
  char *m_end = NULL;
  char *m_arg_ptr = NULL;
  char *m_parser = NULL;
  char m_hostname[HOSTNAME_MAX_LEN] = {0};
  char m_base_hostname[HOSTNAME_MAX_LEN] = {0};
  char *chrptr = NULL;
  char dec_addr[16] = {0};
  char clear_port[6] = {0};
  struct if_ipaddr_list *list_node;
  const char *rhostname;

  int envcount;
  char envstr[1024];
  char tmpstr[1024];
  ELAN_CAPABILITY cap;
  qsw_info_t qinfo;

  signal(SIGINT, SIG_DFL);
  signal(SIGQUIT, SIG_DFL);
  signal(SIGTERM, SIG_DFL);

  /*
   * Remote side sends us a stringified port number to send back
   * stderr on.  If zero, stderr is folded in with stdout.
   */
        
  alarm(60);

  hostname = &m_hostname[0];
  memset(&mbuf[0],0,sizeof(mbuf));

  errno = 0;
  i = 0;
  do { 
    rv = read(0,&clear_port[i],1);
    if (rv != 1) {
      syslog(LOG_ERR, "%s: %m", "mqshd: Bad bad read of stderr port.");
      exit(1);
    }
    i++;
  } while (clear_port[i-1] != '\0');

  cport = strtol(clear_port, (char **)NULL, 10);
  if (errno != 0) {
    syslog(LOG_ERR, "%s: %m", "mqshd: Bad stderr port received.");
    exit(1);
  }

  /* read munge blob for authentication */
  buf_length = fd_null_read_n(0, &mbuf[0], MAX_MBUF_SIZE);
  if (buf_length < 0) {
    syslog(LOG_ERR, "%s: %m", "mqshd: bad read error.");
    errnum = __READ;
    goto error_out;
  }

  /*
   * We call munge_verify which will take what we read in and return a
   * pointer to an unmunged buffer and mungeverify will also fill in our
   * credential structure so we can verify the caller with what he 
   * supplied.
   * 
   * The format of this buffer is as follows (each a string terminated 
   * with a '\0' (null):
   *                                              SIZE            EXAMPLE
   *                                              ==========      =============
   * remote_user_name                             variable        "mhaskell"
   * '\0'
   * dotted_decimal_address_of_this_server        7-15 bytes      "134.9.11.155"
   * '\0'
   * stderr_port_number                           4-8 bytes       "50111"
   * '\0'
   * lrand48()_client_produced_number             1-8 bytes       "1f79ca0e"
   * '\0'
   * users_command                                variable        "ls -al"
   * '\0' '\0'
   *
   */
  mptr = &mbuf[0];
  if ((m_rv = munge_decode(mbuf,0,(void **)&mptr,&buf_length,
                           &cred.pw_uid,&cred.pw_gid)) != EMUNGE_SUCCESS) {
    errnum = __MUNGE;
    goto error_out;
  }

  if ((mptr == NULL) || (buf_length <= 0)) {
    errnum = __PAYLOAD;
    goto error_out;
  }

  /*
   * We get the user supplied id, then verify we should
   * still do this.
   */
  m_arg_char_ctr = strlen(mptr);
  m_parser = m_arg_ptr = mptr;
  m_end = mptr + buf_length;

  if ((pwd = getpwnam(m_arg_ptr)) == NULL) {
    syslog(LOG_ERR, "bad getpwnam(): %m");
    errnum = __SYSTEM;
    goto error_out;
  }

  if (pwd->pw_uid != cred.pw_uid) {
    if (cred.pw_uid != 0) {
      syslog(LOG_ERR, "Permission denied.");
      errnum = __CRED;
      goto error_out;
    }
  }

  if ((rhostname = findhostname(fromp)) == NULL)
      goto error_out;

#ifdef USE_PAM
  retcode = pam_start("mqshell", pwd->pw_name, &conv, &pamh);
  if (retcode != PAM_SUCCESS) {
    syslog(LOG_ERR, "pam_start: %s\n", pam_strerror(pamh, retcode));
    errnum = __AUTH;
    goto error_out;
  }

  pam_set_item (pamh, PAM_RUSER, pwd->pw_name);
  pam_set_item (pamh, PAM_RHOST, rhostname);
  pam_set_item (pamh, PAM_TTY, "mqshell");

  retcode = pam_authenticate(pamh, 0);
  if (retcode == PAM_SUCCESS) {
    retcode = pam_acct_mgmt(pamh, 0);
  }

  if (retcode == PAM_SUCCESS) {
    if (setgid(pwd->pw_gid) != 0) {
      pam_end(pamh, PAM_SYSTEM_ERR);
      errnum = __SYSTEM;
      goto error_out;
    }
    if (initgroups(m_arg_ptr, pwd->pw_gid) != 0) {
      pam_end(pamh, PAM_SYSTEM_ERR);
      errnum = __SYSTEM;
      goto error_out;
    }
    retcode = pam_setcred(pamh, PAM_ESTABLISH_CRED);
  }

  if (retcode == PAM_SUCCESS) {
    retcode = pam_open_session(pamh,0);
  }

  if (retcode != PAM_SUCCESS) {
    pam_end(pamh, retcode);
    syslog(LOG_ERR | LOG_AUTH, "%s\n", pam_strerror(pamh, retcode));
    errnum = __AUTH;
    goto error_out;
  }
#endif

  /*
   * Now we check to make sure that we are the intended host...
   */
  (unsigned int) m_parser += (m_arg_char_ctr+1);
  if (m_parser >= m_end) {
    syslog(LOG_ERR, "parser went beyond valid data: %m");
    errnum = __INTERNAL;
    goto error_out;
  }
  m_arg_char_ctr = strlen(m_parser);

  if (gethostname(&m_hostname[0], HOSTNAME_MAX_LEN) < 0) {
    syslog(LOG_ERR, "failed gethostname: %m");
    errnum = __SYSTEM;
    goto error_out;
  }

  chrptr = strchr(&m_hostname[0],'.');
  if (chrptr == NULL) {
    strncpy(&m_base_hostname[0],&m_hostname[0],sizeof(m_base_hostname));
  } else {
    chrnum = (unsigned int) chrptr - (unsigned int) &m_hostname[0];
    strncpy(&m_base_hostname[0],&m_hostname[0], 
            MIN ((sizeof(m_base_hostname)), (chrnum-1)));
  }
  m_base_hostname[ sizeof(m_base_hostname) - 1 ] = '\0';

  m_hostent = gethostbyname(&m_base_hostname[0]);
  if (m_hostent == NULL) {
    syslog(LOG_ERR, "failed gethostbyname: %m");
    errnum = __SYSTEM;
    goto error_out;
  }

  strncpy(&dec_addr[0], m_parser, sizeof(dec_addr));
  dec_addr[ sizeof(dec_addr) - 1 ] = '\0';

  m_rv = inet_pton(AF_INET, &dec_addr[0], &sin.sin_addr.s_addr);
  if (m_rv <= 0) {
    syslog(LOG_ERR, "failed inet_pton: %m");
    errnum = __SYSTEM;
    goto error_out;
  }

  found = 0;
  while ((h_ptr = (struct in_addr *) *m_hostent->h_addr_list++) != NULL) {
    if (memcmp(&h_ptr->s_addr, &sin.sin_addr.s_addr, m_hostent->h_length) == 0) {
      found++;
      break;
    }
  }

  if (!found) {
    if (get_interface_addresses() < 0)
      goto error_out;

    list_node = list_head;
    while (list_node != NULL && memcmp(&list_node->ipaddr,          
                                       &sin.sin_addr.s_addr, 
                                       m_hostent->h_length) != 0)
      list_node = list_node->next;
    
    if (list_node != NULL)
      found++;

    free_list(list_head);

    if (found == 0) {
      /*
       * Just a bad address...
       */
      errno = EACCES;
      syslog(LOG_ERR, "%s: %m","addresses don't match");
      errnum = __INTERNAL;
      goto error_out; 
    }
  }

  /*
   * Now we grab the desired port from the user for stderr...
   */
  (unsigned int) m_parser += (m_arg_char_ctr+1);
  if (m_parser >= m_end) {
    syslog(LOG_ERR, "parser went beyond valid data: %m");
    errnum = __INTERNAL;
    goto error_out;
  }
  m_arg_char_ctr = strlen(m_parser);

  errno = 0;
  port = strtol(m_parser, (char **)NULL, 10);
  if (port == 0 && errno != 0) {
    syslog(LOG_ERR, "%s: %m", "mqshd: Bad port number from client.");
    errnum = __SYSTEM;
    goto error_out;
  }

  if (port != cport) {
    errnum = __PORT;
    goto error_out;
  }

  /*
   * Now we need to retrieve the random number generated by the client
   * so we can send it back to him on the stderr port.
   */
  (unsigned int) m_parser += (m_arg_char_ctr+1);
  if (m_parser >= m_end) {
    syslog(LOG_ERR, "parser went beyond valid data: %m");
    errnum = __INTERNAL;
    goto error_out;
  }
  m_arg_char_ctr = strlen(m_parser);

  errno = 0;
  rand = strtol(m_parser,(char **)NULL,10);
  if (rand == 0 && errno != 0) {
    syslog(LOG_ERR, "%s: %m", "mqshd: Bad random number from client.");
    errnum = __SYSTEM;
    goto error_out;
  }

  /* Double check to make sure protocol is ok */
  if (cport == 0) {
    if (rand != 0) {
      syslog(LOG_ERR,"protocol error");
      errnum = __PORT;
      goto error_out;
    }
  }

 error_out:
  alarm(0);

  /*
   * We should have a port number now for the stderr, so now we set up the socket
   * for the client.
   */

  sock = 0;
  if (cport != 0) {
    int length;
    char c;

    length = sizeof( struct sockaddr_in );
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
      syslog(LOG_ERR,"%s: %m","can't get stderr port: socket call failed.");
      free(mptr);
      exit(1);
    }
    sin.sin_family = AF_INET;
    sin.sin_port = htons(cport);
    sin.sin_addr.s_addr = fromp->sin_addr.s_addr;
    if (connect(sock, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
      syslog(LOG_ERR,"%s: %m","connect stderr port: failed.");
      free(mptr);
      exit(1);
    }

    /*
     * Need to wait for client to accept stderr socket
     * before proceeding.
     */
    m_rv = read(0,&c,1);
    if (m_rv != 1 || c != '\0') {
      syslog (LOG_ERR, "%s", "mqshd: Client not ready.");
      goto quit_now;
    }
  }

  if (errnum != __NONE) {
    /* If stderr port requested, these errors are sent on the stderr
     * stream.  If stderr not requested, they are sent on the
     * stdout stream.
     */
    switch(errnum) {
    case __READ: errorsock(sock, "mqshd: error reading munge blob."  );
      break;
    case __MUNGE: errorsock(sock,"mqshd: %s", munge_strerror(m_rv));
      break;
    case __PAYLOAD: errorsock(sock,"mqshd: Bad payload data.");
      break;
    case __PORT: errorsock(sock,"mqshd: Cleartext stderr port number"
                           " is not the same as the encrypted one.");
      break;
    case __CRED: errorsock(sock,"mqshd: failed credential check.");
      break;
    case __AUTH: errorsock(sock, "mqshd: failed authentication.");
      break;
    case __SYSTEM: errorsock(sock,"mqshd: internal system error.");
      break;
    case __INTERNAL: errorsock(sock,"mqshd: internal error.");
      break;
    default:
      break;
    }
    
  quit_now:      
    if (errnum != __READ && errnum != __MUNGE && errnum != __PAYLOAD)
      goto bad2;
    goto bad;
  }

  if (cport != 0) {
    /*
     * Write random number to stderr
     */
    rand = htonl(rand);
    rv = fd_write_n(sock,&rand,sizeof(rand));
    if (rv == -1) {
      syslog(LOG_ERR,"%s: %m","couldn't write to stderr port: ");
      error("mqshd: internal system error.");
      goto bad2;
    }
  }

  /*
   * Stderr is all set up, begin reading qshell arguments.
   */ 

  /* read cwd of client - will change to cwd on remote side */
  if (getstr(cwd, sizeof(cwd), "cwd") < 0) {
    goto bad2;
  }
 
  /* read environment of client - will replicate on remote side */
  if (getstr(tmpstr, sizeof(tmpstr), "envcount") < 0) {
    goto bad2;
  }
  envcount = atoi(tmpstr);
  while (envcount-- > 0) {
    if (getstr(envstr, sizeof(envstr), "envstr") < 0) {
      goto bad2;
    }
    /* Following is a mem-leak on some systems, on others it is
     * the proper way to call putenv.
     */
    putenv(strdup(envstr));
  } 
 
  /* read elan capability */
  if (getstr(tmpstr, sizeof(tmpstr), "capability") < 0) {
    goto bad2;
  }
  if (qsw_decode_cap(tmpstr, &cap) < 0) {
    errlog("mqshd: error decoding capability");
    goto bad2;
  } 
 
  for (i = 0; i < qsw_cap_bitmap_count(); i += 16) {
    if (getstr(tmpstr, sizeof(tmpstr), "capability") < 0) { 
      goto bad2;
    }
    if (qsw_decode_cap_bitmap(tmpstr, &cap, i) < 0) {
      errlog("mqshd: error reading capability bitmap(%d): %s", i, tmpstr);
      goto bad2;
    }
  }
 
  /* read info structure */
  if (getstr(tmpstr, sizeof(tmpstr), "qsw info") < 0) {
    goto bad2;
  }

  if (qsw_decode_info(tmpstr, &qinfo) < 0) { 
    errlog("mqshd: error decoding qsw info.");
    goto bad2;
  }

  /* 
   * End qshell arguments.
   */ 

  /*
   * Now we are ready to process the user's command.
   */

  /* get command */
  (unsigned int) m_parser += (m_arg_char_ctr+1);
  if (m_parser >= m_end) {
    syslog(LOG_ERR, "parser went beyond valid data: %m");
    error("mqshd: internal error.");
    goto bad2;
  }
  m_arg_char_ctr = strlen(m_parser);

  if (m_arg_char_ctr <= (ARG_MAX+1)) {
    strncpy(&cmdbuf[0], m_parser, sizeof(cmdbuf));
    cmdbuf[ sizeof(cmdbuf) - 1 ] = '\0';
    free(mptr);
  } else {
    errno = EOVERFLOW;
    error("%s: not enough space allocated for command args: %s\n", 
          &m_hostname[0], strerror(errno));
    goto bad2;
  }

  if (pwd->pw_uid != 0 && !access(_PATH_NOLOGIN, F_OK)) {
    error("Logins are currently disabled.\n");
    goto bad;
  }

  m_rv = write(1, "", 1);
  if (m_rv != 1) {
    errlog("mqshd: bad write of null to stdout: %s\n", strerror(errno));
    exit(1);
  }
  sent_null = 1;

  if (pwd->pw_uid == 0) paranoid = 1;

  /*
   * Fork off a process to send back stderr if requested.
   */
  if (port) {
    if (pipe(pv) < 0) {
      error("Can't make pipe.\n");
      goto bad;
    }
    pid = fork();
    if (pid == -1)  {
      error("Can't fork; try again.\n");
      goto bad;
    }
    if (pid) {
      close(0); 
      close(1);
      close(2); 
      close(pv[1]);
      stderr_parent(sock, pv[0], pid);
      /* NOTREACHED */
    }
    setpgrp();
    close(sock); 
    close(pv[0]);
    dup2(pv[1], 2);
    close(pv[1]);
  }

  /*
   * Set up quadrics Elan capabilities and program desc.
   * Fork a couple of times in here.
   * On error, send diagnostics to stderr and exit.
   */
  qsw_setup_program(&cap, &qinfo, pwd->pw_uid);

  /*
   *  Become the locuser, etc. etc. then exec the shell command.
   */

  /* set the path to the shell */
  theshell = pwd->pw_shell;
  if (!theshell || !*theshell) {
    /* shouldn't we deny access? */
    theshell = _PATH_BSHELL;
  }

#if BSD > 43
  if (setlogin(pwd->pw_name) < 0) {
    errlog("setlogin() failed: %s", 
           strerror(errno));
    exit(1);
  }
#endif
#ifndef USE_PAM
  if (setgid(pwd->pw_gid)) {
    errlog("setgid: %s", strerror(errno));
    exit(1);
  }

  if (initgroups(pwd->pw_name, pwd->pw_gid)) {
    errlog("initgroups: %s", strerror(errno));
    exit(1);
  }
#endif

  if (setuid(pwd->pw_uid)) {
    errlog("setuid: %s", strerror(errno));
    exit(1);
  }

  strncat(homedir, pwd->pw_dir, sizeof(homedir)-6);
  homedir[sizeof(homedir)-1] = 0;

  strcat(path, _PATH_DEFPATH);

  strncat(shell, theshell, sizeof(shell)-7);
  shell[sizeof(shell)-1] = 0;

  strncat(username, pwd->pw_name, sizeof(username)-6);
  username[sizeof(username)-1] = 0;

  shellname = strrchr(theshell, '/');
  if (shellname) 
    shellname++;
  else 
    shellname = theshell;

  if (paranoid) {
    syslog(LOG_INFO|LOG_AUTH, 
           "%s@%s as %s: cmd='%s' cwd='%s'",
           remuser, rhostname, locuser, cmdbuf, cwd);
  }

  /* override USER, HOME, SHELL environment vars */
  putenv(username);
  putenv(homedir);
  putenv(shell);

  /* change to client working dir */
  if (chdir(cwd) < 0) {
    errlog("chdir to client working directory: %s: %s\n", strerror(errno), &cwd[0]);
    exit(1);
  }
  /*
   * Close all fds, in case libc has left fun stuff like 
   * /etc/shadow open.
   */
#define FDS_INUSE 2
  for (ifd = getdtablesize()-1; ifd > FDS_INUSE; ifd--) 
    close(ifd);

  execl(theshell, shellname, "-c", cmdbuf, 0);
  errlog("failed to exec shell: %s", strerror(errno));
  exit(1);

 bad2:
  free(mptr);
 bad:
  close(sock);
  exit(1);
}

static void network_init(int fd, struct sockaddr_in *fromp)
{
  struct linger linger;
  socklen_t fromlen;
  int on=1;

  fromlen = sizeof(*fromp);
  if (getpeername(fd, (struct sockaddr *) fromp, &fromlen) < 0) {
    errlog("getpeername: %s\n", strerror(errno));
    exit(1);
  }
  if (keepalive &&
      setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (char *)&on,
                 sizeof(on)) < 0)
    errlog("setsockopt (SO_KEEPALIVE): %s\n", strerror(errno));
  linger.l_onoff = 1;
  linger.l_linger = 60;                   /* XXX */
  if (setsockopt(fd, SOL_SOCKET, SO_LINGER, (char *)&linger,
                 sizeof (linger)) < 0)
    errlog("setsockopt (SO_LINGER): %s\n", strerror(errno));

  if (fromp->sin_family != AF_INET) {
    errlog("malformed \"from\" address (af %d): %s\n", 
           fromp->sin_family, strerror(errno));
    exit(1);
  }

#ifdef IP_OPTIONS
  {
    u_char optbuf[BUFSIZ/3], *cp;
    char lbuf[BUFSIZ+1], *lp;
    socklen_t optsize = sizeof(optbuf);
    int  ipproto;
    struct protoent *ip;

    if ((ip = getprotobyname("ip")) != NULL)
      ipproto = ip->p_proto;
    else
      ipproto = IPPROTO_IP;
    if (!getsockopt(0, ipproto, IP_OPTIONS, (char *)optbuf, &optsize) &&
        optsize != 0) {
      lp = lbuf;

      /*
       * If these are true, this will not run off the end of lbuf[].
       */
      assert(optsize <= BUFSIZ/3);
      assert(3*optsize <= BUFSIZ);
      for (cp = optbuf; optsize > 0; cp++, optsize--, lp += 3)
        snprintf(lp, 4, " %2.2x", *cp);

      syslog(LOG_NOTICE,
             "Connection received from %s using IP options"
             " (ignored): %s",
             inet_ntoa(fromp->sin_addr), lbuf);

      if (setsockopt(0, ipproto, IP_OPTIONS, NULL, optsize) != 0) {
        syslog(LOG_ERR, "setsockopt IP_OPTIONS NULL: %m");
        exit(1);
      }
    }
  }
#endif
}


int
main(int argc, char *argv[])
{
  int ch;
  struct sockaddr_in from;
  _check_rhosts_file=1;

  openlog("mqshd", LOG_PID | LOG_ODELAY, LOG_DAEMON);
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
      syslog(LOG_ERR, "usage: mqshd [-%s]", OPTIONS);
      exit(2);
    }
  }
  argc -= optind;
  argv += optind;

#ifdef USE_PAM
  if (_check_rhosts_file == 0 || allow_root_rhosts)
    syslog(LOG_ERR, "-l and -h functionality has been moved to "
           "pam_rhosts_auth in /etc/pam.conf");
#endif /* USE_PAM */

  network_init(0, &from);
 
  if (qsw_init() < 0) {
      syslog(LOG_ERR, "qsw_init failed. Exiting...");
      exit(1);
  }
  qsw_spawn_neterr_thr();

 doit(&from);

  qsw_fini();

  return 0;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

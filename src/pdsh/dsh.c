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

/* 
 * Theory of operation:
 *
 * The main thread creates a separate thread for each rsh/krsh/etc. which lasts 
 * the life of the connection (establishing it, copying remote stdout/stderr  
 * to local stdout/stderr and closing the connection).  The main thread makes 
 * sure that at most fanout number of threads are active at any given time.  
 * When a thread terminates, it signals a condition variable (threadcount_cond)
 * which causes the main thread to start another rsh/krsh/etc. thread to take 
 * its place.
 *
 * We rely on implicit stdio locking to enable us to write lines to 
 * stdout/stderr from multiple threads concurrently without getting the lines 
 * all mixed up.
 * 
 * A special watchdog thread sends SIGLARM to any threads that have been in 
 * the DSH_RCMD state (usually connect() in rcmd/k4cmd/etc.) for more than 
 * CONNECT_TIMEOUT seconds.  SIGALRM is masked everywhere but during connect().
 * Similarly, if a command timeout is specified (default is none), the watchdog
 * thread sends SIGALRM to threads that have been in the DSH_READING state
 * too long.
 *
 * When a user types ^C, the resulting SIGINT invokes a handler which lists
 * threads in the DSH_READING state.  If another SIGINT is received within
 * INTR_TIME secs (default 1 sec), pdsh terminates.
 * 
 * All the state for a thread is contained in thd_t struct.  An array of
 * these structures is declared globally so signal handlers can access.
 * The array is initialized by dsh() below, and the rsh() function for each
 * thread is passed the element corresponding to one connection.
 */

#if     HAVE_CONFIG_H
#include "config.h"
#endif

#if	HAVE_PTHREAD_H
#include <pthread.h>
#endif
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/poll.h>
#include <sys/time.h>
#if	HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/mman.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if	HAVE_STRINGS_H
#include <strings.h>            /* FD_SET calls bzero on aix */
#endif
#include <errno.h>
#include <assert.h>
#include <netdb.h>              /* gethostbyname */
#include <sys/resource.h>       /* get/setrlimit */

#ifndef PTHREAD_STACK_MIN
#  define PTHREAD_STACK_MIN ((size_t) sysconf (_SC_THREAD_STACK_MIN))
#endif

#define dsh_mutex_lock(pmutex)                                                \
 do {                                                                         \
      if ((errno = pthread_mutex_lock (pmutex)))                              \
           errx ("%s:%d: mutex_lock: %m", __FILE__, __LINE__);                \
  } while (0)

#define dsh_mutex_unlock(pmutex)                                              \
 do {                                                                         \
      if ((errno = pthread_mutex_unlock (pmutex)))                            \
           errx ("%s:%d: mutex_unlock: %m", __FILE__, __LINE__);              \
  } while (0)


/* set the default stacksize for threads to 128k */
#define DSH_THREAD_STACKSIZE    128*1024

#include "src/common/list.h"
#include "src/common/xmalloc.h"
#include "src/common/xstring.h"
#include "src/common/err.h"
#include "src/common/xpoll.h"
#include "src/common/fd.h"
#include "dsh.h"
#include "opt.h"
#include "pcp_client.h"
#include "pcp_server.h"
#include "wcoll.h"
#include "rcmd.h"

static int debug = 0;

/*
 * Mutex and condition variable for implementing `fanout'.  When a thread
 * terminates, it decrements threadcount and signals threadcount_cond.
 * The main, once it has spawned the fanout number of threads, suspends itself
 * until a thread termintates.
 */
static pthread_mutex_t threadcount_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t threadcount_cond = PTHREAD_COND_INITIALIZER;
static int threadcount = 0;

/*
 * This array is initialized in dsh().  It contains an entry for every
 * potentially active thread, though only the fanout number will be active
 * at once.  It is out here in global land so the signal handler for ^C can
 * report which hosts are blocked.
 */
static thd_t *t;
static pthread_mutex_t thd_mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * Timeout values, initialized in dsh(), used in _wdog().
 */
static int connect_timeout, command_timeout;

/*
 * Terminate on a single SIGINT (batch mode)
 */
static int sigint_terminates = 0;

/*
 *  Buffered output prototypes:
 */
typedef void (* out_f) (const char *, ...);
static int _do_output (int fd, cbuf_t cb, out_f outf, bool read_rc, thd_t *t);
static int _handle_rcmd_stderr (thd_t *t);
static int _handle_rcmd_stdout (thd_t *t);
static void _flush_output (cbuf_t cb, out_f outf, thd_t *t);

/*
 * Emulate signal() but with BSD semantics (i.e. don't restore signal to
 * SIGDFL prior to executing handler).
 */
typedef void SigFunc(int signal);

static SigFunc *_xsignal(int signal, SigFunc *handler)
{
    struct sigaction sa, old_sa;

    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, signal);
    sa.sa_flags = 0;
    sigaction(signal, &sa, &old_sa);

    return ((SigFunc *) old_sa.sa_handler);
}

/*
 * SIGALRM handler.  This is just a stub because we are really interested
 * in interrupting connect() in rcmd/k4cmd/etc. or xpoll() below and
 * causing them to return EINTR.
 */
static void _alarm_handler(int dummy)
{
}

/* 
 * Helper function for handle_sigint().  Lists the status of all connected
 * threads.
 */
static void _list_slowthreads(void)
{
    int i;
    time_t ttl;

    dsh_mutex_lock(&thd_mutex);

    for (i = 0; t[i].host != NULL; i++) {

        switch (t[i].state) {
        case DSH_READING:
            err("%p: %S: command in progress", t[i].host);
            ttl = t[i].connect + command_timeout - time(NULL);
            if (debug && command_timeout)
                err(" (timeout in %d secs)\n", ttl);
            else
                err("\n");
            break;
        case DSH_RCMD:
            ttl = t[i].start + connect_timeout - time(NULL);
            err("%p: %S: connecting", t[i].host, ttl);
            if (debug && connect_timeout)
                err(" (timeout in %d secs)\n", ttl);
            else
                err("\n");
            break;
        case DSH_NEW:
            if (debug)
                err("%p: %S: [new]\n", t[i].host);
            break;
        case DSH_FAILED:
            if (debug)
                err("%p: %S: [failed]\n", t[i].host);
            break;
        case DSH_DONE:
            if (debug)
                err("%p: %S: [done]\n", t[i].host);
            break;
        case DSH_CANCELED:
            if (debug)
                err("%p: %S: [canceled]\n", t[i].host);
            break;
        }
    }

    dsh_mutex_unlock(&thd_mutex);
}

/*
 * Block SIGINT SIGTSTP SIGCHLD n this thread.
 */
static void _mask_signals(int how)
{
    sigset_t blockme;

    assert ((how == SIG_BLOCK) || (how == SIG_UNBLOCK));

    sigemptyset(&blockme);
    sigaddset(&blockme, SIGINT);
    sigaddset(&blockme, SIGTSTP);
    sigaddset(&blockme, SIGCHLD);
    pthread_sigmask(how, &blockme, NULL);
}

/*
 * If the underlying rsh mechanism supports it, forward signals to remote 
 * process.
 */
static void _fwd_signal(int signum)
{
    int i;

    dsh_mutex_lock(&thd_mutex);
    for (i = 0; t[i].host != NULL; i++) {
        if (t[i].state == DSH_READING)
            rcmd_signal(t[i].rcmd, signum);
    }
    dsh_mutex_unlock(&thd_mutex);

}

static int _thd_connect_timeout (thd_t *t)
{
    if ((connect_timeout > 0) && (t->start != ((time_t) -1))) {
        if (t->start + connect_timeout < time (NULL))
            return (1);
    }
    return (0);
}

static int _thd_command_timeout (thd_t *t)
{
    if ((command_timeout > 0) && (t->connect != ((time_t) -1))) {
        if (t->connect + command_timeout < time (NULL))
            return (1);
    }
    return (0);
}

/* 
 * Watchdog thread.  Send SIGALRM to 
 *   - threads in connecting state for too long
 *   - threads in connected state for too long (if selected on command line)
 * Sleep for two seconds between polls 
 */
static void *_wdog(void *args)
{
    int i;
    
    for (;;) {

        if (t == NULL) /* We're done */
            return NULL;

        for (i = 0; t[i].host != NULL; i++) {
            switch (t[i].state) {
            case DSH_RCMD:
                if (_thd_connect_timeout (&t[i]))
                        pthread_kill(t[i].thread, SIGALRM);
                break;
            case DSH_READING:
                if (_thd_command_timeout (&t[i]))
                        pthread_kill(t[i].thread, SIGALRM);
                break;
            case DSH_NEW:
            case DSH_DONE:
            case DSH_FAILED:
            case DSH_CANCELED:
                break;
            }
        }
        sleep (WDOG_POLL);
    }
    return NULL;
}

/*
 * Return the h_addr of a hostname, exiting if there is a lookup failure.
 *	name (IN)	hostname
 *	addr (OUT)	pointer to location where address will be written
 */
static void _gethost(char *name, char *addr)
{
    struct hostent *hp;

    if (!(hp = gethostbyname(name)))
            errx("%p: gethostbyname(\"%S\") failed\n", name);
    /* assert(hp->h_addrtype == AF_INET); */
    assert(IP_ADDR_LEN == hp->h_length);
    memcpy(addr, hp->h_addr_list[0], IP_ADDR_LEN);
}

/*
 *  Update thread state to connecting, unless the thread
 *   has been canceled, in which case close fds if they are open
 *   and return DSH_CANCELED.
 */
static state_t _update_connect_state (thd_t *a)
{
    dsh_mutex_lock(&thd_mutex);
    a->connect = time(NULL);
    if (a->state != DSH_CANCELED)
        a->state = DSH_READING;
    dsh_mutex_unlock(&thd_mutex);

    if (a->state == DSH_CANCELED) {
        if (a->rcmd->fd >= 0)
            close (a->rcmd->fd);
        if (a->rcmd->efd >= 0)
            close (a->rcmd->efd);
    }

    return (a->state);
}

static int _pcp_server (thd_t *th)
{
    struct pcp_server svr[1];

    svr->infd =          th->rcmd->fd;
    svr->outfd =         svr->infd;
    svr->preserve =      th->pcp_popt;
    svr->target_is_dir = th->pcp_yopt;
    svr->outfile =       th->outfile_name;

    return (pcp_server (svr));
}

static int _pcp_client (thd_t *th)
{
    struct pcp_client pcp[1];

    pcp->infd =       th->rcmd->fd;
    pcp->outfd =      pcp->infd;

    pcp->preserve =   th->pcp_popt;
    pcp->pcp_client = th->pcp_Zopt;
    pcp->host =       th->host;
    pcp->infiles =    th->pcp_infiles;

    return (pcp_client (pcp));
}

static int _parallel_copy (thd_t *th)
{
    int rv = 0;
    /* 
     * Run threaded pcp server or client
     */
    if (th->pcp_Popt)
        rv = _pcp_server (th);
    else
        rv = _pcp_client (th);

    if ((!th->pcp_Popt && rv < 0) || (th->pcp_Popt)) {
        /* 
         *  Copy any pending stderr to user 
         *   (ignore errors) 
         *
         *  Notes: 
         *
         *  If the pcp_client was executed, stderr is unlikely b/c
         *  the pcp_server does not write to stderr and all
         *  pdsh/pdcp option parameters have been checked for
         *  correctness.  However, under a disaster situation,
         *  pdsh/pdcp core code itself could have an error and
         *  write to stderr.  This call should only be executed if
         *  there is an error with the pcp_client.  Since the
         *  client is responsible for closing/finishing the
         *  connection the below could spin if an error wasn't
         *  available for reading.
         *
         *  If the pcp_server was executed, stderr output could
         *  come from the pdsh/pdcp core code.  For example, if a
         *  file has a permission denied error, it would be caught
         *  in the options checks and output to stderr.  Even if
         *  there is no stderr, the below cannot spin b/c the
         *  pcp_client on the remote node will close the
         *  connection when the copy (or error output) is
         *  complete.
         */
        while (_handle_rcmd_stderr (th) > 0)
            ;
        _flush_output (th->errbuf, (out_f) err, th);

    }

    close(th->rcmd->fd);
    if (th->dsh_sopt)
        close(th->rcmd->efd);

    return (rv);
}

/*
 * Rcp thread.  One per remote connection.
 * Arguments are pointer to thd_t entry defined above.
 */
static void *_rcp_thread(void *args)
{
    thd_t *a = (thd_t *) args;
    int result = DSH_DONE;      /* the desired outcome */
    int rc;
    char *rcpycmd = NULL;

#if	HAVE_MTSAFE_GETHOSTBYNAME
    if (a->rcmd->opts->resolve_hosts)
        _gethost(a->host, a->addr);
#endif
    a->start = time(NULL);
    dsh_mutex_lock(&thd_mutex);
    a->state = DSH_RCMD;
    dsh_mutex_unlock(&thd_mutex);

    /* For reverse copy, the host needs to be appended to the end of the command */
    if (a->pcp_Popt) {
        xstrcat(&rcpycmd, a->cmd);
        xstrcat(&rcpycmd, " ");
        xstrcat(&rcpycmd, a->host);
    }

    rcmd_connect (a->rcmd, a->host, a->addr, a->luser, a->ruser, 
                  (rcpycmd) ? rcpycmd : a->cmd, a->nodeid, a->dsh_sopt);

    if (rcpycmd)
        Free((void **) &rcpycmd);

    if (a->rcmd->fd == -1)
        result = DSH_FAILED;
    else if (_update_connect_state(a) != DSH_CANCELED) 
        _parallel_copy(a);

    /* update status */
    dsh_mutex_lock(&thd_mutex);
    a->state = result;
    a->finish = time(NULL);
    dsh_mutex_unlock(&thd_mutex);

    rc = rcmd_destroy (a->rcmd);
    if ((a->rc == 0) && (rc > 0))
        a->rc = rc;

    /* Signal dsh() so another thread can replace us */
    dsh_mutex_lock(&threadcount_mutex);
    threadcount--;
    pthread_cond_signal(&threadcount_cond);
    dsh_mutex_unlock(&threadcount_mutex);

    return NULL;
}

/* 
 * Extract a remote command return code embedded in output, returning
 * the code as an integer and truncating the line.
 */
static int _extract_rc(char *buf)
{
    int ret = 0;
    char *p = strstr(buf, RC_MAGIC);

    if (p) {
        if (buf[strlen(buf) - 1] == '\n' && p != buf)
            *p++ = '\n';
        *p = '\0';
        p += strlen(RC_MAGIC);
        ret = atoi(p);
    }
    return ret;
}


static int _do_output (int fd, cbuf_t cb, out_f outf, bool read_rc, thd_t *t)
{
    char c;
    int n, rc;
    int dropped = 0;

    if ((rc = cbuf_write_from_fd (cb, fd, -1, &dropped)) < 0) {
        if (errno == EAGAIN)
            return (1);
        err ("%p: %S: read: %m\n", t->host);
        return (-1);
    } 

    /*
     *  Use cbuf_peek_line with a single character buffer in order to
     *   get the buffer size needed for the next line (if any).
     */
    while ((n = cbuf_peek_line (cb, &c, 1, 1))) {
        char *buf;

        if (n < 0) {
            err ("%p: %S: Failed to peek line: %m\n", t->host);
            break;
        }

        /*
         *  Allocate enough space for line plus NUL character,
         *   then actually read line data into buffer:
         */
        buf = Malloc (n + 1);
        if ((n = cbuf_read (cb, buf, n))) {
            if (n < 0) {
                err ("%p: %S: Failed to read line from buffer: %m\n", t->host);
                break;
            }
            if (read_rc)
                t->rc = _extract_rc (buf);
            if (strlen (buf) > 0) {
                /*
                 *  We are careful to use a single call to write the line
                 *   to the output stream to avoid interleaved lines of
                 *   output.
                 */
                if (t->labels)
                    outf ("%S: %s", t->host, buf);
                else
                    outf ("%s", buf);
                fflush (NULL);
            }
        }
        Free ((void **)&buf);
    }

    return (rc);
}

static void _flush_output (cbuf_t cb, out_f outf, thd_t *t)
{
    char buf[8192];

    while (cbuf_read (cb, buf, 8192) > 0) {
        if (t->labels)
            outf ("%S: %s\n", t->host, buf);
        else
            outf ("%s\n", buf);
    }

    return;
}

static int _die_if_signalled (thd_t *t)
{
    int sig;

    if ((sig = (t->rc - 128)) <= 0)
        return (0);

    err ("%p: process on host %S killed by signal %d\n", t->host, sig);
    _fwd_signal (SIGTERM);
    errx ("%p: terminating all processes.\n");

    /* NOTREACHED */
    return (0);
}

static int _handle_rcmd_stdout (thd_t *th)
{
    int rc = _do_output (th->rcmd->fd, th->outbuf, (out_f) out, true, th);

    if (rc <= 0) {
        close (th->rcmd->fd);
        th->rcmd->fd = -1;
    }

    return (rc);
}

static int _handle_rcmd_stderr (thd_t *th)
{
    int rc = _do_output (th->rcmd->efd, th->errbuf, (out_f) err, false, th);

    if (rc <= 0) {
        close (th->rcmd->efd);
        th->rcmd->efd = -1;
    }

    return (rc);
}

/*
 * Rsh thread.  One per remote connection.
 * Arguments are pointer to thd_t entry defined above.
 */
static void *_rsh_thread(void *args)
{
    thd_t *a = (thd_t *) args;
    int rv;
    int result = DSH_DONE;      /* the desired outcome */
    struct xpollfd xpfds[2];
    int nfds = 1;

    a->start = time(NULL);

#if	HAVE_MTSAFE_GETHOSTBYNAME
    if (a->rcmd->opts->resolve_hosts)
        _gethost(a->host, a->addr);
#endif
    _xsignal (SIGPIPE, SIG_BLOCK);

    /* establish the connection */
    dsh_mutex_lock(&thd_mutex);
    a->state = DSH_RCMD;
    dsh_mutex_unlock(&thd_mutex);

    rcmd_connect (a->rcmd, a->host, a->addr, a->luser, a->ruser,
                  a->cmd, a->nodeid, a->dsh_sopt);

    if (a->rcmd->fd == -1) {
        result = DSH_FAILED;    /* connect failed */
    } else if (_update_connect_state(a) != DSH_CANCELED) {

        fd_set_nonblocking (a->rcmd->fd);

        memset (xpfds, 0, sizeof (xpfds));

        /* prep for poll call */
        xpfds[0].fd = a->rcmd->fd;
        if (a->dsh_sopt) {      /* separate stderr */
            fd_set_nonblocking (a->rcmd->efd);
            xpfds[1].fd = a->rcmd->efd;
            nfds++;
        }
        else
            xpfds[1].fd = -1;

        xpfds[0].events = xpfds[1].events = POLLIN; 
#if	STDIN_BCAST             /* not yet supported */
        xpfds[0].events |= POLLOUT;
#endif

        /*
         * poll / read / report loop.
         */
        while (xpfds[0].fd >= 0 || xpfds[1].fd >= 0) {

            /* poll (possibility for SIGALRM) */
            rv = xpoll(xpfds, nfds, -1);
            if (rv == -1) {
                if (errno != EINTR) 
                    err("%p: %S: xpoll: %m\n", a->host);
                else if (_thd_command_timeout (a))
                    err("%p: %S: command timeout\n", a->host);
                else
                    continue; /* interrupted by spurious signal */

                result = DSH_FAILED;
                rcmd_signal (a->rcmd, SIGTERM);
                break;
            }

            /* stdout ready or closed ? */
            if (xpfds[0].revents & (XPOLLREAD|XPOLLERR)) {
                if (_handle_rcmd_stdout (a) <= 0)
                    xpfds[0].fd = -1;
            }

            /* stderr ready or closed ? */
            if (a->dsh_sopt && xpfds[1].revents & (XPOLLREAD|XPOLLERR)) {
                if (_handle_rcmd_stderr (a) <= 0)
                    xpfds[1].fd = -1;
            }

            /* kill parallel job if kill_on_fail and one task was signaled */
            if (a->kill_on_fail) 
                _die_if_signalled (a);

#if	STDIN_BCAST             /* not yet supported */
            /* stdin ready ? */
            if (FD_ISSET(a->rcmd->fd, &writefds)) {
            }
#endif
        }
    }

    /* update status */
    dsh_mutex_lock(&thd_mutex);
    a->state = result;
    a->finish = time(NULL);
    dsh_mutex_unlock(&thd_mutex);

    /* flush any pending output */
    _flush_output (a->outbuf, (out_f) out, a);
    _flush_output (a->errbuf, (out_f) err, a);

    rv = rcmd_destroy (a->rcmd);
    if ((a->rc == 0) && (rv > 0))
        a->rc = rv;

    /* if a single qshell thread fails, terminate whole job */
    if (a->kill_on_fail && a->state == DSH_FAILED) {
        _fwd_signal(SIGTERM);
        errx("%p: terminating all processes\n");
    }

    /* Signal dsh() so another thread can replace us */
    dsh_mutex_lock(&threadcount_mutex);
    threadcount--;
    pthread_cond_signal(&threadcount_cond);
    dsh_mutex_unlock(&threadcount_mutex);
    return NULL;
}

#define TIME_T_YEAR	60*60*24*7*52

/*
 * If debugging, call this to dump thread connect/command times.
 */
static void _dump_debug_stats(int rshcount)
{
    time_t conTot = 0, conMin = TIME_T_YEAR, conMax = 0;
    time_t cmdTot = 0, cmdMin = TIME_T_YEAR, cmdMax = 0;
    int failed = 0;
    int canceled = 0;
    int n;

    for (n = 0; n < rshcount; n++) {
        if (t[n].state == DSH_FAILED) {
            failed++;
            continue;
        }
        if (t[n].state == DSH_CANCELED) {
            canceled++;
            continue;
        }
        assert(t[n].start && t[n].connect && t[n].finish);

        conTot += t[n].connect - t[n].start;
        cmdTot += t[n].finish - t[n].connect;
        conMin = MIN(conMin, t[n].connect - t[n].start);
        conMax = MAX(conMax, t[n].connect - t[n].start);
        cmdMin = MIN(cmdMin, t[n].finish - t[n].connect);
        cmdMax = MAX(cmdMax, t[n].finish - t[n].connect);
    }
    if (rshcount > failed) {
        err("Connect time:  Avg: %d sec, Min: %d sec,  Max: %d sec\n",
            conTot / (rshcount - failed), conMin, conMax);
        err("Command time:  Avg: %d sec, Min: %d sec,  Max: %d sec\n",
            cmdTot / (rshcount - failed), cmdMin, cmdMax);
    } else {
        err("Connect time:  no sucesses\n");
        err("Command time:  no sucesses\n");
    }
    err("Failures:      %d\n", failed);
    if (canceled)
        err("Canceled:      %d\n", canceled);
}

/*
 * Initialize pthread attr pointed to by `attrp' with dsh default
 *  detach state, and stacksize `stacksize.'
 * 
 * Exits program on failure.
 */
static int _dsh_attr_init (pthread_attr_t *attrp, int stacksize)
{
    int rc;

    if (stacksize < PTHREAD_STACK_MIN)
        stacksize = PTHREAD_STACK_MIN;

    if ((rc = pthread_attr_init (attrp))) 
        errx ("pthread_attr_init: %s\n", strerror (rc));
    if ((rc = pthread_attr_setdetachstate (attrp, PTHREAD_CREATE_DETACHED)))
        errx ("pthread_attr_setdetachstate: %s\n", strerror (rc));
    if ((rc = pthread_attr_setstacksize (attrp, stacksize)))
        errx ("pthread_attr_setstacksize: %s\n", strerror (rc));

    return (0);
}

/*
 * Increase nofile limit to maximum if necessary
 */
static void _increase_nofile_limit (opt_t *opt)
{
    struct rlimit rlim[1];
    /*
     *  We'd like to be able to have at least (2*fanout + slop) fds
     *   open at once.
     */
    int nfds = (2 * opt->fanout) + 32;

    if (getrlimit (RLIMIT_NOFILE, rlim) < 0) {
        err ("getrlimit: %m\n");
        return;
    }

    if ((rlim->rlim_cur < rlim->rlim_max) && (rlim->rlim_cur <= nfds)) {
        rlim->rlim_cur = rlim->rlim_max;
        if (setrlimit (RLIMIT_NOFILE, rlim) < 0) 
            err ("Unable to increase max no. files: %m");
    }

    return;
}

static int _thd_init (thd_t *th, opt_t *opt, List pcp_infiles, int i)
{ 
    th->luser = opt->luser;        /* general */
    th->ruser = opt->ruser;
    th->state = DSH_NEW;
    th->labels = opt->labels;
    th->nodeid = i;
    th->cmd = opt->cmd;
    th->dsh_sopt = opt->separate_stderr;  /* dsh-specific */
    th->rc = 0;
    th->pcp_infiles = pcp_infiles;        /* pcp-specific */
    th->pcp_outfile = opt->outfile_name;
    th->pcp_popt = opt->preserve;
    th->pcp_ropt = opt->recursive;
    th->pcp_yopt = opt->target_is_directory;
    th->pcp_Popt = opt->reverse_copy;
    th->pcp_Zopt = opt->pcp_client;
    th->pcp_progname = opt->progname;
    th->outfile_name = opt->outfile_name;
    th->kill_on_fail = opt->kill_on_fail;
    th->outbuf = cbuf_create (64, 131072);
    th->errbuf = cbuf_create (64, 131072);

    if (!(th->rcmd = rcmd_create (th->host))) {
        th->state = DSH_CANCELED;
        return (-1);
    }

#if	!HAVE_MTSAFE_GETHOSTBYNAME
    /* if MT-safe, do it in parallel in rsh/rcp threads */
    /* gethostbyname_r is not very portable so skip it */
    if (th->rcmd->opts->resolve_hosts)
        _gethost(th->host, th->addr);
#endif

    return (0);

}

static int 
_cancel_pending_threads (void)
{
    int n = 0;
    int i;

    if (t == NULL) 
        return (0);

    dsh_mutex_lock (&threadcount_mutex);
    for (i = 0; t[i].host != NULL; i++) {
        if ((t[i].state == DSH_NEW) || (t[i].state == DSH_RCMD)) {
            t[i].state = DSH_CANCELED;
            ++n;
        }
    }
    err ("%p: Canceled %d pending threads.\n", n);
    dsh_mutex_unlock (&threadcount_mutex);

    return (0);
}

/* 
 *  Handle SIGNINT from signals thread. One ^C lists slow threads.
 *   Another ^C within one second aborts the job.
 */
static void 
_handle_sigint(time_t *last_intrp)
{
    if (!t) return;

    if (sigint_terminates) {
        _fwd_signal(SIGINT);
        errx("%p: batch mode interrupt, aborting.\n");
        /* NORETURN */
    } else if (time(NULL) - *last_intrp > INTR_TIME) {
        err("%p: interrupt (one more within %d sec to abort)\n", INTR_TIME);
        err("%p:  (^Z within %d sec to cancel pending threads)\n", INTR_TIME);
        *last_intrp = time(NULL);
        _list_slowthreads();
    } else {
        _fwd_signal(SIGINT);
        errx("%p: interrupt, aborting.\n");
    }
}

/*
 *  ^Z handler. A ^Z within one second of ^C will cancel any
 *    threads that have not started or are still connecting.
 *    Otherwise, ^Z has the default behavior of stopping the process.
 */
static void 
_handle_sigtstp (time_t last_intr)
{
    if (!t) 
        return;
    if (time (NULL) - last_intr > INTR_TIME) 
        raise (SIGSTOP);
    else
        _cancel_pending_threads ();
}

static void * 
_signals_thread (void *arg)
{
    sigset_t set;
    time_t last_intr = 0;
    int signo;
    int e;

    sigemptyset (&set);
    sigaddset (&set, SIGINT);
    sigaddset (&set, SIGTSTP);

    while (t != NULL) {
        if ((e = sigwait (&set, &signo)) != 0) {
            if (e == EINTR) continue;
            err ("sigwait: %s\n", strerror (e));
        }

        switch (signo) {
        case SIGINT:  
            _handle_sigint (&last_intr); 
            break;
        case SIGTSTP: 
            _handle_sigtstp (last_intr); 
            break;
        default:
            err ("%p: Didn't expect to be here.\n");
        }
    }
    return NULL;
}

/* 
 * Run command on a list of hosts, keeping 'fanout' number of connections 
 * active concurrently.
 */
int dsh(opt_t * opt)
{
    int i, rc = 0;
    int rv, rshcount;
    pthread_t thread_wdog;
    pthread_t thread_sig;
    pthread_attr_t attr_wdog;
    pthread_attr_t attr_sig;
    List pcp_infiles = NULL;
    hostlist_iterator_t itr;
    const char *domain = NULL;
    bool domain_in_label = false;

    _mask_signals (SIG_BLOCK);

    /*
     *   Initialize rcmd modules...
     */
    if (rcmd_init(opt) < 0) {
        err("%p: unable to initialize an rcmd module\n");
		exit(1);
    }

    _increase_nofile_limit (opt);

    /* install signal handlers */
    _xsignal(SIGALRM, _alarm_handler);

    if (opt->sigint_terminates)
        sigint_terminates = 1;

    rshcount = hostlist_count(opt->wcoll);

    /* prepend DSHPATH setting to command */
    if (pdsh_personality() == DSH && opt->dshpath) {
        char *cmd = Strdup(opt->dshpath);

        xstrcat(&cmd, opt->cmd);
        Free((void **) &opt->cmd);
        opt->cmd = cmd;
    }

    /* Initialize getstat if needed */
    if (opt->kill_on_fail || opt->ret_remote_rc) 
        opt->getstat = ";echo " RC_MAGIC "$?";

    /* append echo $? to command */
    if (pdsh_personality() == DSH && opt->getstat) {
        char *cmd = Strdup(opt->cmd);

        xstrcat(&cmd, opt->getstat);
        Free((void **) &opt->cmd);
        opt->cmd = cmd;
    }

    /* build PCP command */
    if (pdsh_personality() == PCP && !opt->reverse_copy) {
        char *cmd = NULL;

        /* expand directories, if any, and verify access for all files */
        if (!(pcp_infiles = pcp_expand_dirs(opt->infile_names))) {
            err("%p: unable to build file copy list\n");
            exit(1);
        }

        xstrcat(&cmd, opt->remote_program_path);
        if (opt->recursive)
            xstrcat(&cmd, " -r");
        if (opt->preserve)
            xstrcat(&cmd, " -p");
        if (list_count(pcp_infiles) > 1)     /* outfile must be directory */
            xstrcat(&cmd, " -y");
        xstrcat(&cmd, " -z ");               /* invoke pcp server */
        xstrcat(&cmd, opt->outfile_name);    /* outfile is remote target */

        opt->cmd = cmd;
    }

    if (pdsh_personality() == PCP && opt->reverse_copy) {
        char *cmd = NULL;
        char *filename;
        ListIterator i;

        xstrcat(&cmd, opt->remote_program_path);

        if (opt->recursive)
            xstrcat(&cmd, " -r");
        if (opt->preserve)
            xstrcat(&cmd, " -p");
        xstrcat(&cmd, " -Z ");               /* invoke pcp client */

        i = list_iterator_create(opt->infile_names);
        while ((filename = list_next(i))) {
            xstrcat(&cmd, " ");
            xstrcat(&cmd, filename);
        }
        list_iterator_destroy(i);

        /* The 'host' will be appended to the cmd in _rcp_thread */
        opt->cmd = cmd;
    }

    /* set debugging flag for this module */
    if (opt->debug)
        debug = 1;

    /* build thread array--terminated with t[i].host == NULL */
    t = (thd_t *) Malloc(sizeof(thd_t) * (rshcount + 1));

    if (!(itr = hostlist_iterator_create(opt->wcoll)))
        errx("%p: hostlist_iterator_create failed\n");
    i = 0;
    while ((t[i].host = hostlist_next(itr))) {
        char *d;
        
        assert(i < rshcount);

        _thd_init (&t[i], opt, pcp_infiles, i);

        /*
         * Require domain names in labels if hosts have 
         *  different domains
         */
        if (!domain_in_label && (d = strchr (t[i].host, '.'))) {
            if (domain == NULL)
                domain = d;
            else if (strcmp (d, domain) != 0)
                domain_in_label = true;
        }

        i++;
    }
    assert(i == rshcount);
    hostlist_iterator_destroy(itr);

    if (domain_in_label)
        err_no_strip_domain ();

    /* set timeout values for _wdog() */
    connect_timeout = opt->connect_timeout;
    command_timeout = opt->command_timeout;

    /* start the watchdog thread */
    _dsh_attr_init (&attr_wdog, DSH_THREAD_STACKSIZE);
    rv = pthread_create(&thread_wdog, &attr_wdog, _wdog, (void *) t);

    /* start the signals thread */
    _dsh_attr_init (&attr_sig, DSH_THREAD_STACKSIZE);
    rv = pthread_create(&thread_sig, &attr_sig, _signals_thread, (void *) t);

    /* start all the other threads (at most 'fanout' active at once) */
    for (i = 0; i < rshcount; i++) {

        /* wait until "room" for another thread */
        dsh_mutex_lock(&threadcount_mutex);

        if (opt->fanout == threadcount)
            pthread_cond_wait(&threadcount_cond, &threadcount_mutex);

        /*
         *  Advance past any canceled threads
         */
        while ((t[i].state == DSH_CANCELED) && (i < rshcount))
            ++i;
        /*
         *  Abort if no more threads
         */
        if (i >= rshcount) {
            dsh_mutex_unlock(&threadcount_mutex);
            break;
        }

        /* create thread */
        _dsh_attr_init (&t[i].attr, DSH_THREAD_STACKSIZE);
#ifdef 	PTHREAD_SCOPE_SYSTEM
        /* we want 1:1 threads if there is a choice */
        pthread_attr_setscope(&t[i].attr, PTHREAD_SCOPE_SYSTEM);
#endif
        rv = pthread_create(&t[i].thread, &t[i].attr,
                            pdsh_personality() == DSH
                            ? _rsh_thread : _rcp_thread, (void *) &t[i]);
        if (rv != 0) {
            if (opt->kill_on_fail)
                _fwd_signal(SIGTERM);
            errx("%p: pthread_create %S: %S\n", t[i].host, strerror(rv));
        }
        threadcount++;

        dsh_mutex_unlock(&threadcount_mutex);
    }

    /* wait for termination of remaining threads */
    dsh_mutex_lock(&threadcount_mutex);
    while (threadcount > 0)
        pthread_cond_wait(&threadcount_cond, &threadcount_mutex);

    if (debug)
        _dump_debug_stats(rshcount);

    /*
     * Cancel signals thread and unblock SIGINT/SIGTSTP
     */
    pthread_cancel(thread_sig);
    _mask_signals (SIG_UNBLOCK);

    /* if -S, our exit value is the largest of the return codes */
    if (opt->ret_remote_rc) {
        for (i = 0; t[i].host != NULL; i++) {
            if (t[i].state == DSH_FAILED)
                rc = RC_FAILED;
            if (t[i].rc > rc)
                rc = t[i].rc;
        }
    }

    /*
     *  free hostnames allocated in hostlist_next()
     *   and buffers allocated when initializing thread array
     */
    for (i = 0; t[i].host != NULL; i++) {
        free(t[i].host);
        cbuf_destroy (t[i].outbuf);
        cbuf_destroy (t[i].errbuf);
    }

    Free((void **) &t);         /* cleanup */

    return rc;
}


/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

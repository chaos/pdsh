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
#include <sys/stat.h>
#include <sys/param.h>
#include <dirent.h>
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

#ifndef MAXPATHNAMELEN
#define MAXPATHNAMELEN MAXPATHLEN
#endif

/* set the default stacksize for threads to 128k */
#define DSH_THREAD_STACKSIZE    128*1024

/* define the filename flag as an impossible filename */
#define EXIT_SUBDIR_FILENAME    "a!b@c#d$"
#define EXIT_SUBDIR_FLAG        "E\n"

#include "src/common/list.h"
#include "src/common/xmalloc.h"
#include "src/common/xstring.h"
#include "src/common/err.h"
#include "src/common/xpoll.h"
#include "dsh.h"
#include "opt.h"
#include "wcoll.h"
#include "mod_rcmd.h"

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

/*
 * Timeout values, initialized in dsh(), used in _wdog().
 */
static int connect_timeout, command_timeout;

/*
 *  Buffered output prototypes:
 */
typedef void (* out_f) (const char *, ...);
static int _do_output (int fd, cbuf_t cb, out_f outf, bool read_rc, thd_t *t);
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
 * Helper function for intr_handler().  Lists the status of all connected
 * threads.
 */
static void _list_slowthreads(void)
{
    int i;
    time_t ttl;

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
        }
    }
}

/*
 * Block SIGINT in this thread.
 */
static void _int_block(void)
{
    sigset_t blockme;

    sigemptyset(&blockme);
    sigaddset(&blockme, SIGINT);
    pthread_sigmask(SIG_BLOCK, &blockme, NULL);
}

/*
 * If the underlying rsh mechanism supports it, forward signals to remote 
 * process.
 */
static void _fwd_signal(int signum)
{
    int i;

    for (i = 0; t[i].host != NULL; i++) {
        if ((t[i].state == DSH_READING) && (t[i].efd >= 0)) 
			mod_rcmd_signal(t[i].efd, signum);
    }

}

/* 
 * SIGINT handler.  The program can be terminated by two ^C's within
 * INTR_TIME seconds.  Otherwise, ^C causes a list of connected thread
 * status.  This should only be handled by the "main" thread.  We block
 * SIGINT in other threads.
 */
static void _int_handler(int signum)
{
    static time_t last_intr = 0;

    if (!t) return;

    if (time(NULL) - last_intr > INTR_TIME) {
        err("%p: interrupt (one more within %d sec to abort)\n",
            INTR_TIME);
        last_intr = time(NULL);
        _list_slowthreads();
    } else {
        _fwd_signal(signum);
        errx("%p: interrupt, aborting.\n");
    }
}

/*
 * Simpler version of above for -b "batch mode", i.e. pdsh is run by a
 * script, and when the script dies, we should die too.
 */
static void _int_handler_justdie(int signum)
{
    _fwd_signal(signum);
    errx("%p: batch mode interrupt, aborting.\n");
}

/* 
 * Watchdog thread.  Send SIGALRM to 
 *   - threads in connecting state for too long
 *   - threads in connected state for too long (if selected on command line)
 * Sleep for two seconds between polls (actually sleep for connect_timeout
 * on the first iteration).
 */
static void *_wdog(void *args)
{
    int i;
    
    _int_block();               /* block SIGINT */

    sleep (connect_timeout);

    for (;;) {

        if (t == NULL) /* We're done */
            return NULL;

        for (i = 0; t[i].host != NULL; i++) {
            switch (t[i].state) {
            case DSH_RCMD:
                if ((connect_timeout > 0) && (t[i].start != ((time_t) -1))) {
                    if (t[i].start + connect_timeout < time(NULL))
                        pthread_kill(t[i].thread, SIGALRM);
                }
                break;
            case DSH_READING:
                if ((command_timeout > 0) && (t[i].connect != ((time_t) -1))) {
                    if (t[i].connect + command_timeout < time(NULL))
                        pthread_kill(t[i].thread, SIGALRM);
                }
                break;
            case DSH_NEW:
            case DSH_DONE:
            case DSH_FAILED:
                break;
            }
        }
        sleep (WDOG_POLL);
    }
    return NULL;
}

static void _rexpand_dir(List list, char *name)
{
    DIR *dir;
    struct dirent *dp;
    struct stat sb;
    char file[MAXPATHNAMELEN];

    dir = opendir(name);
    if (dir == NULL)
        errx("%p: opendir: %s: %m\n", name);
    while ((dp = readdir(dir))) {
        if (dp->d_ino == 0)
            continue;
        if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, ".."))
            continue;
        snprintf(file, sizeof(file), "%s/%s", name, dp->d_name);
        if (stat(file, &sb) < 0)
            errx("%p: can't stat %s: %m\n", file);
        if (access(name, R_OK) < 0)
            errx("%p: access: %s: %m\n", name);
        if (!S_ISDIR(sb.st_mode) && !S_ISREG(sb.st_mode))
            errx("%p: not a regular file or directory: %s\n", file);
        list_append(list, Strdup(file));
        if (S_ISDIR(sb.st_mode))
            _rexpand_dir(list, file);
    }
    closedir(dir);

    /* Since pdcp reads file names and directories only once for
     * efficiency, we must specify a special flag so we know when
     * to tell the server to "move up" the directory tree.
     */

    list_append(list, Strdup(EXIT_SUBDIR_FILENAME));
}

static List _expand_dirs(List infiles)
{
    List new = list_create(NULL);
    struct stat sb;
    char *name;
    ListIterator i;

    i = list_iterator_create(infiles);
    while ((name = list_next(i))) {
        if (access(name, R_OK) < 0)
            errx("%p: access: %s: %m\n", name);
        if (stat(name, &sb) < 0)
            errx("%p: stat: %s: %m\n", name);
        list_append(new, name);
        /* -r option checked during command line argument checks */
        if (S_ISDIR(sb.st_mode))
            _rexpand_dir(new, name);
    }
    
    return new;
}

/*
 * Wrapper for the write system call that handles short writes.
 * Not sure if write ever returns short in practice but we have to be sure.
 *	fd (IN)		file descriptor to write to 
 *	buf (IN)	data to write
 *	size (IN)	size of buf
 *	RETURN		-1 on failure, size on success
 */
static int _rcp_write(int fd, char *buf, int size)
{
    char *bufp = buf;
    int towrite = size;
    int outbytes;

    while (towrite > 0) {
        outbytes = write(fd, bufp, towrite);
        if (outbytes <= 0) {
            assert(outbytes != 0);
            return -1;
        }
        towrite -= outbytes;
        bufp += outbytes;
    }
    return size;
}

/*
 * Write the contents of the named file to the specified file descriptor.
 *	outfd (IN)	file descriptor to write to 
 *	filename (IN)	name of file
 *	host (IN)	name of remote host for error messages
 *	RETURN		-1 on failure, 0 on success.
 */
static int _rcp_send_file_data(int outfd, char *filename, char *host)
{
    int infd, inbytes, total = 0;
    char tmpbuf[BUFSIZ];

    infd = open(filename, O_RDONLY);
    /* checked ahead of time - shouldn't happen */
    if (infd < 0) {
        err("%S: _rcp_send_file_data: open %s: %m\n", host, filename);
        return -1;
    }
    do {
        inbytes = read(infd, tmpbuf, BUFSIZ);
        if (inbytes < 0) {
            err("%S: _rcp_send_file_data: read %s: %m\n", host, filename);
            return -1;
        }
        if (inbytes > 0) {
            total += inbytes;
            if (_rcp_write(outfd, tmpbuf, inbytes) < 0) {
                err("%S: _rcp_send_file_data: write: %m\n", host);
                return -1;
            }
        }
    } while (inbytes > 0);      /* until EOF */
    close(infd);
    return 0;
}

/*
 * Send string to the specified file descriptor.  Do not send trailing '\0'
 * as RCP terminates strings with newlines.
 *	fd (IN)		file descriptor to write to
 *	str (IN)	string to write
 *	host (IN)	name of remote host for error messages
 *	RETURN 		-1 on failure, 0 on success
 */
static int _rcp_sendstr(int fd, char *str, char *host)
{
    int n;
    assert(strlen(str) > 0);
    assert(str[strlen(str) - 1] == '\n');

    if ((n = _rcp_write(fd, str, strlen(str))) < 0) 
        return -1;

    assert(n == strlen(str));
    return 0;
}

/*
 * Receive an RCP response code and possibly error message.
 *	fd (IN)		file desciptor to read from
 *	host (IN)	hostname for error messages
 *	RETURN		-1 on fatal error, 0 otherwise
 */
static int _rcp_response(int fd, char *host)
{
    char resp;
    int i = 0, result = -1;
    int n;
    char errstr[BUFSIZ];

    if ((n = read(fd, &resp, sizeof(resp))) != sizeof(resp)) 
        return (-1);

    switch (resp) {
        case 0:                /* ok */
            result = 0;
            break;
        default:               /* just error string */
            errstr[i++] = resp;
        case 1:                /* non-fatal error + string */
        case 2:                /* fatal error + string */
            for (; i < BUFSIZ; i++) {
                if (read(fd, &errstr[i], 1) != 1)
                    break;
                if (errstr[i] == '\n')
                    break;
            }
            errstr[i] = '\0';
            if (resp != 1)
                result = 0;
    }
    return result;
}

#define RCP_MODEMASK (S_ISUID|S_ISGID|S_ISVTX|S_IRWXU|S_IRWXG|S_IRWXO)

static int _rcp_sendfile(int fd, char *file, char *host, bool popt)
{
    int result = 0;
    char tmpstr[BUFSIZ], *template;
    struct stat sb;

    /*err("%S: %s\n", host, file); */

    if (stat(file, &sb) < 0) {
        err("%S: %s: %m\n", host, file);
        goto fail;
    }

    if (popt) {
        /* 
         * 1: SEND stat time: "T%ld %ld %ld %ld\n" 
         *    (st_mtime, st_mtime_usec, st_atime, st_atime_usec)
         */
        snprintf(tmpstr, sizeof(tmpstr), "T%ld %ld %ld %ld\n",
                 (long) sb.st_mtime, 0L, sb.st_atime, 0L);
        if (_rcp_sendstr(fd, tmpstr, host) < 0)
            goto fail;

        /* 2: RECV response code */
        if (_rcp_response(fd, host) < 0)
            goto fail;
    }

    if (S_ISDIR(sb.st_mode)) {
        /* 
         * 3a: SEND directory mode: "D%04o %d %s\n"
         *     (st_mode & RCP_MODEMASK, 0, name)
         */
        snprintf(tmpstr, sizeof(tmpstr), "D%04o %d %s\n",
                 sb.st_mode & RCP_MODEMASK, 0, xbasename(file));
        if (_rcp_sendstr(fd, tmpstr, host) < 0)
            goto fail;
    } else {
        /* 
         * 3b: SEND file mode: "C%04o %lld %s\n" or "C%04o %ld %s\n"
         *    (st_mode & MODE_MASK, st_size, basename(filename))
         *    Use second template if sizeof(st_size) > sizeof(long).
         */
        template = (sizeof(sb.st_size) > sizeof(long)
                    ? "C%04o %lld %s\n" : "C%04o %ld %s\n");
        snprintf(tmpstr, sizeof(tmpstr), template,
                 sb.st_mode & RCP_MODEMASK, sb.st_size, xbasename(file));
        if (_rcp_sendstr(fd, tmpstr, host) < 0)
            goto fail;
    }

    /* 4: RECV response code */
    if (_rcp_response(fd, host) < 0)
        goto fail;

    if (S_ISREG(sb.st_mode)) {
        /* 5: SEND data */
        if (_rcp_send_file_data(fd, file, host) < 0)
            goto fail;

        /* 6: SEND NULL byte */
        if (_rcp_write(fd, "", 1) < 0)
            goto fail;

        /* 7: RECV response code */
        if (_rcp_response(fd, host) < 0)
            goto fail;
    }

    result = 1;                 /* indicate success */
  fail:
    return result;
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
 * Rcp thread.  One per remote connection.
 * Arguments are pointer to thd_t entry defined above.
 */
static void *_rcp_thread(void *args)
{
    thd_t *a = (thd_t *) args;
    int result = DSH_DONE;      /* the desired outcome */
    int *efdp = a->dsh_sopt ? &a->efd : NULL;

    _int_block();               /* block SIGINT */

#if	HAVE_MTSAFE_GETHOSTBYNAME
    if (a->resolve_hosts)
        _gethost(a->host, a->addr);
#endif
    a->start = time(NULL);
    a->state = DSH_RCMD;

    a->fd = mod_rcmd(a->host, a->addr, a->luser, a->ruser, 
                     a->cmd, a->nodeid, efdp);
    if (a->fd == -1)
        result = DSH_FAILED;
    else {
        a->connect = time(NULL);
        a->state = DSH_READING;

        /* 0: RECV response code */
        if (_rcp_response(a->fd, a->host) >= 0) {
            ListIterator i;
            char *name;

            /* Send the files */
            i = list_iterator_create(a->pcp_infiles);
            while ((name = list_next(i))) {
                if (strcmp(name, EXIT_SUBDIR_FILENAME) == 0) {
                    if (_rcp_sendstr(a->fd, EXIT_SUBDIR_FLAG, a->host) < 0)
                        errx("%p: failed to send exit subdir flag\n");
                    if (_rcp_response(a->fd, a->host) < 0)
                        errx("%p: failed to exit subdir properly\n");
                    continue;
                }
                _rcp_sendfile(a->fd, name, a->host, a->pcp_popt);
            }
            list_iterator_destroy(i);
        } else {
            err("%p: %S: Failed to initiate RCP protocol.\n", a->host);
            /* 
             *  Copy any pending stderr to user 
             *   (ignore errors) 
             */
            while (_do_output (a->efd, a->errbuf, (out_f) err, false, a) > 0)
                ;
            _flush_output (a->errbuf, (out_f) err, a);
        }

        close(a->fd);
        if (a->dsh_sopt)
            close(a->efd);
    } 
    /* update status */
    a->state = result;
    a->finish = time(NULL);

    /* Signal dsh() so another thread can replace us */
    pthread_mutex_lock(&threadcount_mutex);
    threadcount--;
    pthread_cond_signal(&threadcount_cond);
    pthread_mutex_unlock(&threadcount_mutex);

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
    char buf[8192];
    int rc = 0;
    int dropped = 0;

    if ((rc = cbuf_write_from_fd (cb, fd, -1, &dropped)) < 0) {
        if (errno == EAGAIN)
            return (1);
        err ("%p: %S: read: %m\n", t->host);
        return (-1);
    } else if (rc == 0) 
        close (fd);

    while (cbuf_read_line (cb, buf, 8192, 1) > 0) {

        if (read_rc)
            t->rc = _extract_rc (buf);

        if ((buf != NULL) && (strlen (buf) > 0)) {
            if (t->labels)
                outf ("%S: %s", t->host, buf);
            else
                outf ("%s", buf);
            fflush (NULL);
        }
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

/*
 * Rsh thread.  One per remote connection.
 * Arguments are pointer to thd_t entry defined above.
 */
static void *_rsh_thread(void *args)
{
    thd_t *a = (thd_t *) args;
    int rv;
    int result = DSH_DONE;      /* the desired outcome */
    int *efdp = a->dsh_sopt ? &a->efd : NULL;
    struct xpollfd xpfds[2];
    int nfds = 1;

    _int_block();               /* block SIGINT */

    a->start = time(NULL);

#if	HAVE_MTSAFE_GETHOSTBYNAME
    if (a->resolve_hosts)
        _gethost(a->host, a->addr);
#endif
    _xsignal (SIGPIPE, SIG_BLOCK);

    /* establish the connection */
    a->state = DSH_RCMD;

    a->fd = mod_rcmd(a->host, a->addr, a->luser, a->ruser,
                     a->cmd, a->nodeid, efdp);

    /* 
     * Copy stdout/stderr to local stdout/stderr, 
     * appropriately tagged.
     */
    if (a->fd == -1) {
        result = DSH_FAILED;    /* connect failed */
    } else {
        /* connected: update status for watchdog thread */
        a->connect = time(NULL);
        a->state = DSH_READING;

        fd_set_nonblocking (a->fd);

        /* prep for poll call */
        xpfds[0].fd = a->fd;
        if (a->dsh_sopt) {      /* separate stderr */
            fd_set_nonblocking (a->efd);
            xpfds[1].fd = a->efd;
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
                if (errno == EINTR)
                    err("%p: %S: command timeout\n", a->host);
                else
                    err("%p: %S: xpoll: %m\n", a->host);
                result = DSH_FAILED;
                break;
            }

            /* stdout ready or closed ? */
            if (xpfds[0].revents & XPOLLREAD) {
                if (_do_output (a->fd, a->outbuf, (out_f) out, true, a) <= 0)
                    xpfds[0].fd = -1;
            }

            /* stderr ready or closed ? */
            if (a->dsh_sopt && xpfds[1].revents & XPOLLREAD) {
                if (_do_output (a->efd, a->errbuf, (out_f) err, false, a) <= 0)
                    xpfds[1].fd = -1;
            }

            /* 
             *  Check for POLLERR or POLLHUP error conditions from poll().
             *   Simply close the affected fd in case of error
             */
            if (xpfds[0].fd >= 0 && (xpfds[0].revents & XPOLLERR)) {
                close (xpfds[0].fd); /* Ignore errors */
                xpfds[0].fd = -1;
            }
            if (xpfds[1].fd >= 0 && (xpfds[1].revents & XPOLLERR)) {
                close (xpfds[1].fd); /* Ignore errors */
                xpfds[1].fd = -1;
            }

            /* kill parallel job if kill_on_fail and one task was signaled */
            if (a->kill_on_fail) 
                _die_if_signalled (a);

#if	STDIN_BCAST             /* not yet supported */
            /* stdin ready ? */
            if (FD_ISSET(a->fd, &writefds)) {
            }
#endif
        }
    }

    /* update status */
    a->state = result;
    a->finish = time(NULL);

    /* flush any pending output */
    _flush_output (a->outbuf, (out_f) out, a);
    _flush_output (a->errbuf, (out_f) err, a);

    /* if a single qshell thread fails, terminate whole job */
    if (a->kill_on_fail && a->state == DSH_FAILED) {
        _fwd_signal(SIGTERM);
        errx("%p: terminating all processes\n");
    }

    /* Signal dsh() so another thread can replace us */
    pthread_mutex_lock(&threadcount_mutex);
    threadcount--;
    pthread_cond_signal(&threadcount_cond);
    pthread_mutex_unlock(&threadcount_mutex);
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
    int n;

    for (n = 0; n < rshcount; n++) {
        if (t[n].state == DSH_FAILED) {
            failed++;
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

/* 
 * Run command on a list of hosts, keeping 'fanout' number of connections 
 * active concurrently.
 */
int dsh(opt_t * opt)
{
    int i, rc = 0;
    int rv, rshcount;
    pthread_t thread_wdog;
    pthread_attr_t attr_wdog;
    List pcp_infiles = NULL;
    hostlist_iterator_t itr;
    SigFunc *old_int_handler = NULL;


    /*
     *   Initialize rcmd modules...
     */
    if (mod_rcmd_init(opt) < 0) {
        err("%p: unable to initialize an rcmd module\n");
		exit(1);
    }

    _increase_nofile_limit (opt);

    /* install signal handlers */
    _xsignal(SIGALRM, _alarm_handler);
    if (opt->sigint_terminates)
        old_int_handler = _xsignal(SIGINT, _int_handler_justdie);
    else
        old_int_handler = _xsignal(SIGINT, _int_handler);

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
    if (pdsh_personality() == PCP) {
        char *cmd = NULL;

        /* expand directories, if any, and verify access for all files */
        pcp_infiles = _expand_dirs(opt->infile_names);

        xstrcat(&cmd, opt->path_progname); 
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

    /* set debugging flag for this module */
    if (opt->debug)
        debug = 1;

    /* build thread array--terminated with t[i].host == NULL */
    t = (thd_t *) Malloc(sizeof(thd_t) * (rshcount + 1));

    if (!(itr = hostlist_iterator_create(opt->wcoll)))
        errx("%p: hostlist_iterator_create failed\n");
    i = 0;
    while ((t[i].host = hostlist_next(itr))) {
        assert(i < rshcount);
        t[i].luser = opt->luser;        /* general */
        t[i].ruser = opt->ruser;
        t[i].state = DSH_NEW;
        t[i].labels = opt->labels;
        t[i].fd = t[i].efd = -1;
        t[i].nodeid = i;
        t[i].cmd = opt->cmd;
        t[i].dsh_sopt = opt->separate_stderr;  /* dsh-specific */
        t[i].rc = 0;
        t[i].pcp_infiles = pcp_infiles;        /* pcp-specific */
        t[i].pcp_outfile = opt->outfile_name;
        t[i].pcp_popt = opt->preserve;
        t[i].pcp_ropt = opt->recursive;
        t[i].pcp_progname = opt->progname;
        t[i].kill_on_fail = opt->kill_on_fail;
        t[i].outbuf = cbuf_create (64, 8192);
        t[i].errbuf = cbuf_create (64, 8192);
#if	!HAVE_MTSAFE_GETHOSTBYNAME
        /* if MT-safe, do it in parallel in rsh/rcp threads */
        /* gethostbyname_r is not very portable so skip it */
        if (opt->resolve_hosts)
            _gethost(t[i].host, t[i].addr);
#endif
        i++;
    }
    assert(i == rshcount);
    hostlist_iterator_destroy(itr);

    /* set timeout values for _wdog() */
    connect_timeout = opt->connect_timeout;
    command_timeout = opt->command_timeout;

    /* start the watchdog thread */
    _dsh_attr_init (&attr_wdog, DSH_THREAD_STACKSIZE);
    rv = pthread_create(&thread_wdog, &attr_wdog, _wdog, (void *) t);

    /* start all the other threads (at most 'fanout' active at once) */
    for (i = 0; i < rshcount; i++) {

        /* wait until "room" for another thread */
        pthread_mutex_lock(&threadcount_mutex);

        if (opt->fanout == threadcount)
            pthread_cond_wait(&threadcount_cond, &threadcount_mutex);

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

        pthread_mutex_unlock(&threadcount_mutex);
    }

    /* wait for termination of remaining threads */
    pthread_mutex_lock(&threadcount_mutex);
    while (threadcount > 0)
        pthread_cond_wait(&threadcount_cond, &threadcount_mutex);

    if (debug)
        _dump_debug_stats(rshcount);

    /*
     * Reinstall old handler for SIGINT since _int_handler
     *  will segfault once we start freeing thd info structures.
     */
    _xsignal(SIGINT, old_int_handler);

    /* if -S, our exit value is the largest of the return codes */
    if (opt->ret_remote_rc) {
        for (i = 0; t[i].host != NULL; i++) {
            out ("%p: %s: RC=%d\n", t[i].host, t[i].rc);
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

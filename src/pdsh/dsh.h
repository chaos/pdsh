/* 
 * $Id$ 
 *
 * Copyright (C) 2000 Regents of the University of California
 * See ./DISCLAIMER
 */

#ifndef _DSH_INCLUDED
#define _DSH_INCLUDED

#include "conf.h"

#if HAVE_PTHREAD_H
#include <pthread.h>
#endif

#include "list.h"
#include "opt.h"

#define DFLT_FANOUT 		64

#if	NEED_STDERR
#define DFLT_SEPARATE_STDERR	true
#else
#define DFLT_SEPARATE_STDERR	false
#endif

#define LINEBUFSIZE     	2048

#define CONNECT_TIMEOUT 	10	/* secs */
#define INTR_TIME		1 	/* secs */
#define WDOG_POLL 		2	/* secs */

#ifdef HAVE_SDRGETOBJECTS
#define MAX_SP_NODES 		512
#define MAX_SP_NODES_PER_FRAME	16
#define MAX_SP_NODE_NUMBER (MAX_SP_NODES * MAX_SP_NODES_PER_FRAME - 1)
#endif

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 64
#endif

#ifndef MAX
#define MAX(a,b)	((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a,b)	((a) < (b) ? (a) : (b))
#endif

#if !HAVE_PTHREAD_SIGMASK && HAVE_SIGTHREADMASK
#define pthread_sigmask(x, y, z)	sigthreadmask(x, y, z)
#endif

#ifndef _BOOL_DEFINED
#define _BOOL_DEFINED
typedef enum {false, true} bool;
#endif

typedef enum {DSH_NEW, DSH_RCMD, DSH_READING, DSH_DONE, DSH_FAILED} state_t;

typedef struct thd {
        pthread_t	thread;
        pthread_attr_t	attr;
        state_t		state;      		/* thread state */
        char 		*host;     		/* host name */
        char 		*luser;    		/* local username */
        char 		*ruser;    		/* remote username */
        rcmd_t 		rcmd_type;		/* K4, BSD ? */
        time_t 		start;   		/* time stamp for start */
        time_t 		connect; 		/* time stamp for connect */
        time_t 		finish;  		/* time stamp for finish */

        char 		*dsh_cmd;      		/* command */
        bool 		dsh_sopt;       	/* true if -s (sep stderr/out)*/

	list_t 		pcp_infiles;		/* name of input files/dirs */
	char 		*pcp_outfile;		/* name of output file/dir */
	bool 		pcp_popt;		/* preserve mtime/mode */
	bool 		pcp_ropt;		/* recursive */
	int		rc;			/* remote return code (-S) */
	int		nodeid;			/* node index */
	int		nnodes;			/* number of nodes in job */
	int		fd;			/* stdin/stdout */
	int		efd;			/* signal/stderr */
} thd_t;

struct hostent *xgethostbyname(const char *);
int dsh(opt_t *);
void set_rcmd_timeout(int);

int xrcmd(char *, char *, char *, char *, int, int *);
void xrcmd_signal(int, int);
void xrcmd_init(opt_t *);

int k4cmd (char *, char *, char *, char *, int, int *);
void k4cmd_signal(int, int);
void k4cmd_init(opt_t *);

int sshcmd (char *, char *, char *, char *, int, int *);
int sshcmdrw (char *, char *, char *, char *, int, int *);
void sshcmd_signal(int, int);
void sshcmd_init(opt_t *);

int qcmd(char *, char *, char *, char *, int, int *);
void qcmd_init(opt_t *);
void qcmd_signal(int, int);

#endif /* _DSH_INCLUDED */

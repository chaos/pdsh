/*****************************************************************************\
 *  $Id: $
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

#if     HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/param.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <stdio.h>
#if	HAVE_PTHREAD_H
#include <pthread.h>
#endif
#include <netinet/in.h>
#include <arpa/inet.h>

#include <signal.h>
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <netdb.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <pwd.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>

#include "src/common/err.h"
#include "src/common/list.h"
#include "src/common/xpoll.h"
#include "src/common/xmalloc.h"
#include "src/pdsh/dsh.h"
#include "src/pdsh/mod.h"
#include "src/pdsh/privsep.h"

#define RSH_PORT 514

#if STATIC_MODULES
#  define pdsh_module_info xcpucmd_module_info
#  define pdsh_module_priority xcpucmd_module_priority
#endif    

int pdsh_module_priority = DEFAULT_MODULE_PRIORITY;

static int xcpucmd_init(opt_t *);
static int xcpucmd_signal(int, void *, int);
static int xcpucmd(char *, char *, char *, char *, char *, int, int *, void **); 

/* 
 * Export pdsh module operations structure
 */
struct pdsh_module_operations xcpucmd_module_ops = {
  (ModInitF)       NULL, 
  (ModExitF)       NULL, 
  (ModReadWcollF)  NULL,
  (ModPostOpF)     NULL,
};

/*
 *  Export rcmd module operations
 */
struct pdsh_rcmd_operations xcpucmd_xcpucmd_ops = {
    (RcmdInitF)  xcpucmd_init,
    (RcmdSigF)   xcpucmd_signal,
    (RcmdF)      xcpucmd,
};

/* 
 * Export module options
 */
struct pdsh_module_option xcpucmd_module_options[] = 
 { 
   PDSH_OPT_TABLE_END
 };

/* 
 * Xxcpucmd module info 
 */
struct pdsh_module pdsh_module_info = {
  "rcmd",
  "xcpu",
  "Jim Garlick <garlick@llnl.gov>",
  "XCPU connect method",
  DSH | PCP,
  &xcpucmd_module_ops,
  &xcpucmd_xcpucmd_ops,
  &xcpucmd_module_options[0],
};

static int xcpucmd_init(opt_t * opt)
{
    /* not implemented */
    return 0;
}

static int xcpucmd_signal(int efd, void *arg, int signum)
{
    /* not implemented */
    return 0;
} 

#define PREFIX  "/mnt/xcpu"
#define SCRIPT  "#!/bin/bash\nexec $*\nexit 1\n"

static FILE *
openclone(char *hostname, int *sidp)
{
    char path[MAXPATHLEN];
    FILE *f;

    sprintf(path, "%s/%s/xcpu/clone", PREFIX, hostname);
    f = fopen(path, "r");
    if (f == NULL) 
        err("%s: %m\n", path);
    else if (fscanf(f, "%x", sidp) != 1) {
        err("error reading %s\n", path);
        fclose(f);
        f = NULL;
    }

    return f;
}

static int
openfilefd(char *hostname, int sid, mode_t mode, char *name)
{
    char path[MAXPATHLEN];
    int fd;

    sprintf(path, "%s/%s/xcpu/%x/%s", PREFIX, hostname, sid, name);
    fd = open(path, mode);
    if (fd < 0)
        err("%s: %m\n", path);

    return fd;
}

static int
writefile(char *hostname, int sid, char *name, char *data)
{
    char path[MAXPATHLEN];
    FILE *f;
    int res = 0;

    sprintf(path, "%s/%s/xcpu/%x/%s", PREFIX, hostname, sid, name);
    f = fopen(path, "w");
    if (f == NULL) {
        err("%s: %m\n", path);
        goto done;
    }
    if (fprintf(f, "%s", data) != strlen(data)) {
        err("error writing to %s\n", path);
        goto done;
    }
    res = 1;
done:
    if (f)
        fclose(f);
    return res;
}

static int
_xcpucmd(char *hostname, char *cmd, int *fd2p)
{
    int sid;
    FILE *fclone = NULL;
    int fd = -1;
    char *argstr;

    /* Make a copy of cmd with "xcpu" prepended as arg[0].
     */
    argstr = Malloc(strlen(cmd) + 6);
    sprintf(argstr, "xcpu %s", cmd);

    /* Establish a session by reading its number (sid) from the clone file.
     */
    fclone = openclone(hostname, &sid);
    if (fclone == NULL)
        goto done;
    /* don't close it yet - this preserves session */

    if (writefile(hostname, sid, "exec", SCRIPT) == 0)
        goto done;
    if (writefile(hostname, sid, "argv", argstr) == 0)
        goto done;
    if (writefile(hostname, sid, "ctl", "exec") == 0)
        goto done;

    fd = openfilefd(hostname, sid, O_RDWR, "io");
    if (fd >= 0) {
        if (fd2p)
            *fd2p = openfilefd(hostname, sid, O_RDONLY, "stderr");
    }
done:
    if (argstr)
        Free((void **)&argstr);
    if (fclone)
        fclose(fclone);
        
    return fd; /* session goes away when fd is closed */
}

/*
 * The xcpucmd call itself.
 * 	ahost (IN)	remote hostname
 *	addr (IN)	4 byte internet address
 *	locuser (IN)	local username
 *	remuser (IN)	remote username
 *	cmd (IN)	command to execute
 *	rank (IN)	MPI rank for this process
 *	fd2p (IN/OUT)	if non-NULL, open stderr backchannel on this fd
 *	s (RETURN)	socket for stdout or -1 on failure
 */
static int
xcpucmd(char *ahost, char *addr, char *locuser, char *remuser,
      char *cmd, int rank, int *fd2p, void **arg)
{
    if (strcmp(locuser, remuser) != 0) {
        err("remote user must match local user for xcpu rcmd method\n");
        return -1;
    }
    return _xcpucmd(ahost, cmd, fd2p);
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

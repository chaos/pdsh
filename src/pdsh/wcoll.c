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

#if     HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <assert.h>
#if     HAVE_UNISTD_H
#include <unistd.h>             /* for R_OK, access() */
#endif
#include <stdlib.h>             /* atoi */

#include "dsh.h"
#include "err.h"
#include "list.h"
#include "xmalloc.h"
#include "xstring.h"
#include "xpopen.h"
#include "wcoll.h"
#include "hostlist.h"

/* 
 * Read wcoll from specified file or from the specified FILE pointer.
 * (one of the arguments must be NULL).  
 *      file (IN)       name of wcoll file (or NULL)
 *      f (IN)          FILE pointer to wcoll file (or NULL)    
 *      RETURN          new list containing hostnames
 */
hostlist_t read_wcoll(char *file, FILE * f)
{
    char buf[LINEBUFSIZE], *p;
    hostlist_t new = hostlist_create("");
    FILE *fp = NULL;

    assert(f != NULL || file != NULL);
    if (!new)
        errx("%p: hostlist_create failed\n");

    if (f == NULL) {            /* read_wcoll("file", NULL) */
        if (access(file, R_OK) == -1 || !(fp = fopen(file, "r")))
            errx("%p: %s: %m\n", file);
    } else                      /* read_wcoll(NULL, fp) */
        fp = f;

    while (fgets(buf, LINEBUFSIZE, fp) != NULL) {
        /* zap text following comment char and whitespace */
        if ((p = strchr(buf, '#')) != NULL)
            *p = '\0';
        xstrcln(buf, NULL);

        if (hostlist_push(new, buf) == 0)
            err("%p: warning: target '%s' not parsed\n", buf);
    }
    if (f == NULL)
        fclose(fp);

    return new;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

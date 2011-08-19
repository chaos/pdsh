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

#if     HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <assert.h>
#if     HAVE_UNISTD_H
#include <unistd.h>             /* for R_OK, access() */
#endif
#include <stdlib.h>             /* atoi */
#include <errno.h>
#include <ctype.h>
#include <libgen.h>

#include "src/common/err.h"
#include "src/common/list.h"
#include "src/common/xmalloc.h"
#include "src/common/xstring.h"
#include "src/common/hostlist.h"
#include "src/common/split.h"
#include "src/common/xstring.h"
#include "xpopen.h"
#include "dsh.h"
#include "wcoll.h"

struct wcoll_ctx {
    hostlist_t hl;
    List path_list;
    List include_cache;
};

static void free_f (void *x)
{
    Free (&x);
}

static int strcmp_f (char *a, char *b)
{
    return (strcmp (a, b) == 0);
}

static struct wcoll_ctx * wcoll_ctx_create (const char *path)
{
    char *copy = Strdup (path);
    struct wcoll_ctx *ctx = Malloc (sizeof (*ctx));

    if (!copy || !ctx)
        errx ("%p: wcoll_ctx_create: Out of memory\n");

    ctx->hl = hostlist_create ("");
    ctx->path_list = list_split (":", copy);
    ctx->include_cache = list_create ((ListDelF) free_f);

    Free ((void **) &copy);
    return (ctx);
}

static void wcoll_ctx_destroy (struct wcoll_ctx *ctx)
{
    list_destroy (ctx->path_list);
    list_destroy (ctx->include_cache);
    /*
     * Do not destroy hostlist, it is pulled out of ctx and returned
     *  to caller of read_wcoll()
     */
    ctx->hl = NULL;
    Free ((void **)&ctx);
}


static int wcoll_ctx_file_is_cached (struct wcoll_ctx *ctx, char *file)
{
    if (list_find_first (ctx->include_cache, (ListFindF) strcmp_f, file))
        return 1;
    list_push (ctx->include_cache, Strdup (file));
    return 0;
}

/*
 *  Find first file named [name] in colon-separated path [path]
 *   Returns file path in supplied buffer [buf] of length [n].
 *   Returns -1 on failure.
 */
static int wcoll_ctx_path_lookup (struct wcoll_ctx *ctx,
        const char *name, char *buf, int len)
{
    int rc = -1;
    ListIterator i;
    const char *s;

    if (name == NULL)
        return -1;

    i = list_iterator_create (ctx->path_list);
    while ((s = list_next (i))) {
        int n = snprintf (buf, len, "%s/%s", s, name);
        if (n < 0 || n >= len) {
            errno = ENOSPC;
            break;
        }
        if (access (buf, R_OK) >= 0) {
            rc = 0;
            break;
        }
    }
    list_iterator_destroy (i);

    return (rc);
}

static char * include_file (char *p)
{
    if (strncmp (p, "#include", 8) != 0)
        return NULL;

    p += 8;
    /*
     *  Skip whitespace
     */
    while (isblank (*p)) p++;

    return strtok (p, "\n\r\t ");
}

/*
 *   Return absolute path to [file] in buffer [buf] of max length [len].
 *
 *   If [file] argument is an absolute path or relative to '.' or '..'
 *    then [file] is copied to [buf], otherwise, colon-separated [path]
 *    is searched for a file named [file].
 *
 *   If no file is found, then NULL is returned, otherwise a pointer
 *    to the passed [buf] is returned.
 */
static char * wcoll_ctx_resolve_path (struct wcoll_ctx *ctx,
        const char *file, char *buf, int len)
{
    if (file[0] == '/')
        strncpy (buf, file, len);
    else if ( file[0] == '.'
            && (file[1] == '/' || (file[1] == '.' && file[2] == '/')))
       strncpy (buf, file, len);
    else {
        if (wcoll_ctx_path_lookup (ctx, file, buf, len) < 0)
            return NULL;
    }
    return buf;
}

static int wcoll_ctx_read_file (struct wcoll_ctx *ctx, const char *f);

static int wcoll_ctx_read_line (struct wcoll_ctx *ctx, char *line)
{
    char *p;
    char *included;

    /*
     *  Check for comment. Zap the comment and the rest of the line
     *   unless this is an include statement '#include foo'
     */
    if ((p = strchr(line, '#')) != NULL) {
        if (p == line && (included = include_file (p)) != NULL)
            wcoll_ctx_read_file (ctx, included);
        *p = '\0';
    }
    xstrcln(line, NULL);

    if ((line[0] != '\0') && (hostlist_push(ctx->hl, line) == 0))
        err("%p: warning: target '%s' not parsed\n", line);

    return 0;
}

static int wcoll_ctx_read_stream (struct wcoll_ctx *ctx, FILE *fp)
{
    char buf [LINEBUFSIZE];

    assert (ctx != NULL);
    assert (fp != NULL);

    while (fgets(buf, LINEBUFSIZE, fp) != NULL)
        wcoll_ctx_read_line (ctx, buf);
    return 0;
}


/*
 *  Append the contents of file [f], optionally searching [path], to
 *   the supplied hostlist [hl], returning the result. If [hl] is NULL,
 *   then a new hostlist is allocated.
 *
 *  This function supports
 *
 *    #include file
 *
 *  to include other working collective files.
 */
static int wcoll_ctx_read_file (struct wcoll_ctx *ctx, const char *f)
{
    char fq_path [4096];
    FILE *fp = NULL;

    assert (ctx != NULL);
    assert (f != NULL);

    if (wcoll_ctx_resolve_path (ctx, f, fq_path, sizeof (fq_path)) == NULL)
        errx("%p: %s: %m\n", f);

    /*
     *  Detect recursive #include:
     */
    if (wcoll_ctx_file_is_cached (ctx, fq_path)) {
        err("%p: warning: file '%s' included multiple times\n", f);
        return -1;
    }

    if (access(fq_path, R_OK) == -1 || !(fp = fopen(fq_path, "r")))
        errx("%p: %s: %m\n", f);

    wcoll_ctx_read_stream (ctx, fp);

    fclose(fp);

    return 0;
}

hostlist_t read_wcoll_path (const char *path, const char *file)
{
    struct wcoll_ctx *ctx;
    hostlist_t hl;

    ctx = wcoll_ctx_create (path);
    wcoll_ctx_read_file (ctx, file);
    hl = ctx->hl;
    wcoll_ctx_destroy (ctx);

    return hl;
}

/*
 *  Get the dirname for the file path [file] and copy into the buffer
 *   [dir] of length [len]. If [file] is NULL then return ".".
 */
static char * get_file_path (const char *file, char *dir, int len)
{
    char *str;
    char *dname;

    memset (dir, 0, len);
    dir[0] = '.';

    if (file == NULL)
        return (dir);

    str = Strdup (file);
    dname = dirname (str);

    if (dname && strlen (dname) < len - 1)
        strcpy (dir, dname);
    else
        err ("%p: %s: Error reading file path\n");

    Free ((void **) &str);
    return (dir);
}

/*
 * Read wcoll from specified file or from the specified FILE pointer.
 * (one of the arguments must be NULL).  
 *      file (IN)       name of wcoll file (or NULL)
 *      f (IN)          FILE pointer to wcoll file (or NULL)    
 *      RETURN          new list containing hostnames
 */
hostlist_t read_wcoll(char *file, FILE * f)
{
    char path[4096];
    hostlist_t new;
    struct wcoll_ctx *ctx;
    FILE *fp = NULL;

    assert(f != NULL || file != NULL);

    if (strcmp (file, "-") == 0) {
        f = stdin;
        file = NULL;
    }

    if (f == NULL) {            /* read_wcoll("file", NULL) */
        if (access(file, R_OK) == -1 || !(fp = fopen(file, "r")))
            errx("%p: %s: %m\n", file);
    } else                      /* read_wcoll(NULL, fp) */
        fp = f;

    get_file_path (file, path, sizeof (path));

    ctx = wcoll_ctx_create (path);
    wcoll_ctx_read_stream (ctx, fp);
    new = ctx->hl;
    wcoll_ctx_destroy (ctx);

    return new;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

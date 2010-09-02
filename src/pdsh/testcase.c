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
 * Internal unit tests called by DejaGNU.
 */

#if	HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include "src/common/err.h"
#include "src/common/xmalloc.h"
#include "src/common/xstring.h"
#include "src/common/pipecmd.h"
#include "src/common/fd.h"
#include "dsh.h"

typedef enum { FAIL, PASS } testresult_t;
typedef testresult_t((*testfun_t) (void));
typedef struct {
    char *desc;
    testfun_t fun;
} testcase_t;

static testresult_t _test_xstrerrorcat(void);
static testresult_t _test_pipecmd(void);

static testcase_t testcases[] = {
    /* 0 */ {"xstrerrorcat", &_test_xstrerrorcat},
    /* 1 */ {"pipecmd",      &_test_pipecmd},
};

static void _testmsg(int testnum, testresult_t result)
{
    out("%P: Test %d: %s: %s\n", testnum, testcases[testnum].desc,
        result == PASS ? "PASS" : "FAIL");
}

static testresult_t _test_xstrerrorcat(void)
{
    int e;
    testresult_t result = PASS;

    for (e = 1; e < 100; e++) {
        char *s1 = NULL;
        char *s2 = strerror(e);
        errno = e;
        xstrerrorcat(&s1);
        if (strcmp(s1, s2) != 0) {
            err ("xsterrorcat (errno=%d) = \"%s\" (should be \"%s\")\n", e, s1, s2);

            result = FAIL;
        }
        Free((void **) &s1);
    }
    return result;
}

static testresult_t _test_pipecmd(void)
{
    const char expected[] = "host=foo0 user=foouser n=0";
    const char *args[] = { "host=%h", "user=%u", "n=%n", NULL };

    int n;
    char buf [1024];
    pipecmd_t p;

    if (!(p = pipecmd ("/bin/echo", args, "foo0", "foouser", 0)))
        return FAIL;

    if ((n = fd_read_n (pipecmd_stdoutfd (p), buf, sizeof (buf))) < 0)
        return FAIL;

    buf [n-1] = '\0';

    if (strcmp (expected, buf)) {
        err ("testcase: pipecmd: expected \"%s\" got \"%s\"\n", expected, buf);
        return FAIL;
    }

    pipecmd_wait (p, NULL);
    pipecmd_destroy (p);

    return PASS;
}

void testcase(int testnum)
{
    testresult_t result;

    if (testnum < 0 || testnum >= (sizeof(testcases) / sizeof(testcase_t)))
        errx("%P: Test %d unknown\n", testnum);
    result = testcases[testnum].fun();
    _testmsg(testnum, result);
    exit(0);
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

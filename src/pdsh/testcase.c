/*****************************************************************************\
 *
 *  $Id$
 *  $Source$
 *
 *  Copyright (C) 1998-2002 The Regents of the University of California.
 *  Produced at Lawrence Livermore National Laboratory (cf, DISCLAIMER).
 *  Written by Jim Garlick (garlick@llnl.gov>
 *  UCRL-CODE-980038
 *  
 *  This file is part of PDSH, a parallel remote shell program.
 *  For details, see <http://www.llnl.gov/linux/pdsh/>.
 *  
 *  PDSH is free software; you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *  
 *  PDSH is distributed in the hope that it will be useful, but WITHOUT 
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License 
 *  for more details.
 *  
 *  You should have received a copy of the GNU General Public License along
 *  with PDSH; if not, write to the Free Software Foundation, Inc.,
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

#include "err.h"
#include "dsh.h"
#include "xmalloc.h"
#include "xstring.h"

typedef enum { FAIL, PASS } testresult_t;
typedef testresult_t ((*testfun_t)(void));
typedef struct {
	char *desc;
	testfun_t fun;
} testcase_t;

static testresult_t _test_xstrerrorcat(void);

static testcase_t testcases[] = {
	/* 0 */ { "xstrerrorcat", &_test_xstrerrorcat },
};

static void
_testmsg(int testnum, testresult_t result)
{
	out("%P: Test %d: %s: %s\n", testnum, testcases[testnum].desc,
			result == PASS ? "PASS" :  "FAIL");
}

static testresult_t
_test_xstrerrorcat(void)
{
	testresult_t result = PASS;

	for (errno = 1; errno < 100; errno++) {
#if	HAVE_STRERROR
		char *s2 = strerror(errno);
#else
		extern char *sys_errlist[];
		char *s2 = sys_errlist[errno];
#endif
		char *s1 = NULL;

		xstrerrorcat(&s1);
		if (strcmp(s1, s2) != 0)
			result = FAIL;
		Free((void **)&s1);
	}
	return result;
}

void 
testcase(int testnum)
{
	testresult_t result;

	if (testnum < 0 || testnum >= (sizeof(testcases)/sizeof(testcase_t)))
		errx("%P: Test %d unknown\n", testnum);
	result = testcases[testnum].fun();
	_testmsg(testnum, result);
	exit(0);
}

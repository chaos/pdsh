/* 
 * $Id$ 
 * 
 * Copyright (C) 2000-2002 Regents of the University of California
 * See ./DISCLAIMER
 *
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

static testresult_t test_xstrerrorcat(void);

static testcase_t testcases[] = {
	/* 0 */ { "xstrerrorcat", &test_xstrerrorcat },
};

static void
testmsg(int testnum, testresult_t result)
{
	out("%P: Test %d: %s: %s\n", testnum, testcases[testnum].desc,
			result == PASS ? "PASS" :  "FAIL");
}

static testresult_t
test_xstrerrorcat(void)
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
	testmsg(testnum, result);
	exit(0);
}

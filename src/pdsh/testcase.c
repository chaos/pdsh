#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>

#include "dsh.h"

/*
 * Verify that strerrorcat works (gnats:35)
 */
void 
test1(void)
{
	char *buf = NULL;

	errno = ESRCH;
	xstrerrorcat(&buf);
	printf("Test 1: ESRCH error string is `%s'\n", buf);
	Free(&buf);
}

void 
testcase(int casenum)
{
	switch (casenum) {
		case 1:
			test1();
			break;
		default:
			fprintf(stderr, "Unknown test case: %d\n", casenum);
			exit(0);
	}
	exit(0);
}

#if	HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "dsh.h"

/*
 * Verify that strerrorcat works (gnats:35)
 */
void 
test1(void)
{
	int pass = 1;

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
			pass = 0;
		Free(&s1);
	}
	printf("Test 1: %s\n", pass ? "PASS" : "FAIL");
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

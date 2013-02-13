#!/bin/sh

test_description='pdsh mrsh module tests'

. ${srcdir:-.}/test-lib.sh

if ! test_have_prereq MOD_RCMD_MRSH; then
	skip_all='skipping mrsh tests, mrsh module not available'
	test_done
fi

if ! pdsh -SRmrsh -w localhost /bin/true 2>&1 >/dev/null; then
	skip_all='skipping mrsh tests, mrsh server not available on localhost'
	test_done
fi

test_expect_success 'mrsh module runs' '
	OUTPUT=$(pdsh -Rmrsh -w localhost echo i am here)
'
test_debug '
	echo Output: "$OUTPUT"
'
test_expect_success 'mrsh localhost works' '
	echo "$OUTPUT" | grep "localhost: i am here"
'
test_expect_success  'mrsh: -S generates empty lines (Issue 54)' '
	OUTPUT=$(pdsh -Rmrsh -w localhost -S cd ..)
	[ -z "$OUTPUT" ]
'
test_debug '
	echo Output: "$OUTPUT"
'

test_done

#!/bin/sh

test_description='Issue 36: pdsh truncates long lines

Test that pdsh does not truncate very long lines'

. ${srcdir:-.}/test-lib.sh

test_expect_success 'pdsh does not truncate very long lines' '
	dd if=/dev/urandom bs=1024 count=100 | base64 | xargs -n10 > testfile &&
	pdsh -w foo -N -Rexec cat testfile > output &&
	test_cmp testfile output
'
test_expect_success 'pdsh does not truncate even longer lines' '
	dd if=/dev/urandom bs=1024 count=100 | base64 | xargs -n100 > testfile &&
	pdsh -w foo -N -Rexec cat testfile > output &&
	test_cmp testfile output
'

test_done

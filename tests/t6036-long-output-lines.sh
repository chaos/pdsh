#!/bin/sh

test_description='Issue 36: pdsh truncates long lines

Test that pdsh does not truncate very long lines'

. ${srcdir:-.}/test-lib.sh

if which base64 >/dev/null; then
    base64="base64"
elif which openssl >/dev/null; then
    base64="openssl base64"
else
    skip_all 'failed to find base64 program'
fi


test_expect_success 'pdsh does not truncate very long lines' "
	dd if=/dev/urandom bs=1024 count=100 | $base64 | tr -d '\n' | fold -w8000 > testfile &&
	echo >>testfile &&
	pdsh -w foo -N -Rexec cat testfile > output &&
	test_cmp testfile output
"
test_expect_success 'pdsh does not truncate even longer lines' "
	dd if=/dev/urandom bs=1024 count=100 | $base64 | tr -d '\n' | fold -w80000 > testfile2 &&
        echo >>testfile2 &&
	pdsh -w foo -N -Rexec cat testfile2 > output2 &&
	test_cmp testfile2 output2
"

test_done

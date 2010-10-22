#!/bin/sh

test_description='pdsh internal testcases

Run pdsh internal testsuite'

. ${srcdir:-.}/test-lib.sh

test_expect_success 'working xstrerrorcat' '
	pdsh -T0
'
test_expect_success 'working pipecmd' '
	pdsh -T1
'
test_done

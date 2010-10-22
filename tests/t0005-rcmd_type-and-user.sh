#!/bin/sh

test_description='pdsh rcmd_type and remote user handling'

. ${srcdir:-.}/test-lib.sh

test_expect_success 'pdsh -l sets username for all hosts' '
	pdsh -S -Rexec -lbar -w foo[1-100] test "%u" = bar
'
test_expect_success 'Can set remote username via user@hosts' '
	pdsh -S -Rexec -w bar@foo[1-100] test "%u" = bar
'
test_expect_success 'user@hosts works for a subset of hosts' '
	pdsh -S -Rexec -w u1@foo,u2@bar sh -c \
		"if test %h = foo; then test %u = u1; else test %u = u2; fi"
'
test_expect_success 'Can set rcmd_type via rcmd_type:hosts' '
    PDSH_RCMD_TYPE=ssh
	pdsh -S -w exec:foo[1-10] true
'
test_expect_success 'Can set rcmd_type and user via rcmd_type:user@hosts' '
    PDSH_RCMD_TYPE=ssh
	pdsh -S -w exec:bar@foo[1-10] test "%u" = bar
'
test_done

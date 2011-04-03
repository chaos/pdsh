#!/bin/sh

test_description='pdsh ssh module tests'

. ${srcdir:-.}/test-lib.sh

if ! test_have_prereq MOD_RCMD_SSH; then
	skip_all='skipping ssh tests, ssh module not available'
	test_done
fi

#
#  Create ssh wrapper script that echoes its own arguments. This allows
#   the following tests to run without ssh installed
#

test_expect_success 'create ssh dummy script' '
	echo "#!/bin/sh"  >ssh &&
	echo "echo \"\$@\"" >>ssh &&
	chmod 755 ssh
'

export PATH=.:$PATH
unset PDSH_SSH_ARGS PDSH_SSH_ARGS_APPEND

test_expect_success 'ssh module runs' '
	OUTPUT=$(pdsh -Rssh -w foo command)
'
test_debug '
	echo Output: "$OUTPUT"
'
test_expect_success 'ssh dummy script works' '
	echo "$OUTPUT" | grep "foo: .*foo command"
'
test_expect_success 'ssh works with DSHPATH' '
	OUTPUT=$(DSHPATH=/test/path pdsh -Rssh -w foo command) &&
	echo "$OUTPUT"  | grep "foo: .*foo PATH=/test/path; *command"
'
test_debug '
	echo Output: "$OUTPUT"
'
test_expect_success 'have ssh connect timeout option' '
	SSH_CONNECT_TIMEOUT_OPTION=$(sed -ne "s/#define SSH_CONNECT_TIMEOUT_OPTION \"\(.*\)\"/\1/p" ${PDSH_BUILD_DIR}/config.h) &&
	test -n "$SSH_CONNECT_TIMEOUT_OPTION"
'
test_debug '
	echo "SSH_CONNECT_TIMEOUT_OPTION=\"$SSH_CONNECT_TIMEOUT_OPTION\""
'
test_expect_success 'ssh works with connect timeout' '
	OPT=$(printf -- "$SSH_CONNECT_TIMEOUT_OPTION" 55) &&
	OUTPUT=$(pdsh -t55 -Rssh -w foo command) &&
	echo "$OUTPUT" | grep -- "$OPT"
'
test_debug '
	echo Output: "$OUTPUT"
'
test_expect_success 'ssh and pdcp work together' '
	ln -s $PDSH_BUILD_DIR/src/pdsh/pdsh pdcp && touch jnk &&
	OUTPUT=$(pdcp -Rssh -w foo jnk /tmp 2>&1 || :) &&
	echo "$OUTPUT" | grep "pdcp -z /tmp"
'
test_debug '
	echo Output: "$OUTPUT"
'
test_expect_success 'PDSH_SSH_ARGS works' '
	OUTPUT=$(PDSH_SSH_ARGS="ssh -p 922 -l%u %h" pdsh -luser -Rssh -wfoo hostname) &&
	echo "$OUTPUT" | grep "[-]p 922 -luser foo hostname"
'
test_debug '
	echo Output: "$OUTPUT"
'
test_expect_success 'PDSH_SSH_ARGS_APPEND works' '
	OUTPUT=$(PDSH_SSH_ARGS_APPEND="-p 922" pdsh -Rssh -wfoo hostname) &&
	echo "$OUTPUT" | grep "[-]p 922"
'
test_debug '
	echo Output: "$OUTPUT"
'
test_expect_success 'interactive mode works with ssh (Issue 14)' '
	OUTPUT=$(echo test command line | pdsh -Rssh -wfoo)
	echo "$OUTPUT" | grep "test command line"
'
test_debug '
	echo Output: "$OUTPUT"
'
#
#  Exit code tests:
#
#  If adding new general tests for ssh module, place above here,
#   as the ssh dummy script is rewritten for exit code specific testing.
#

test_expect_success 'create ssh dummy script for exit code testing' '
	echo "#!/bin/bash"  >ssh
	echo "# Usage: $0 -n <this rank> -i <failing rank> -e <exitcode> " >>ssh
	echo "while getopts \":n:i:e:\" opt; do "                          >>ssh
	echo "  case \$opt in"                                             >>ssh
	echo "    n) RANK=\$OPTARG ;;"                                     >>ssh
	echo "    i) FAILRANK=\$OPTARG ;;"                                 >>ssh
	echo "    e) EXITCODE=\$OPTARG ;;"                                 >>ssh
	echo "  esac"                                                      >>ssh
	echo "done"                                                        >>ssh
	echo ""                                                            >>ssh
	echo "if test \$RANK -eq \$FAILRANK; then exit \$EXITCODE; fi"     >>ssh
	echo "exit 0"                                                      >>ssh
	chmod 755 ssh
'
test_expect_success 'ssh dummy script is functional' '
	TEST_EXIT_CODE=$(random 254)
	echo "$TEST_EXIT_CODE"
	ssh -n 1 -i 0 &&
	test_expect_code "$TEST_EXIT_CODE" ssh -n 1 -i 1 -e $TEST_EXIT_CODE
	test_expect_code 0               ssh -n0 -i255 -e $TEST_EXIT_CODE
'
test_expect_success 'ssh works with pdsh -S' '
	TEST_EXIT_CODE=$(random 254) &&
	export PDSH_SSH_ARGS="-n%n -i0 -e$TEST_EXIT_CODE"
	test_expect_code "$TEST_EXIT_CODE" pdsh -Rssh -S -w foo0 command
'
unset PDSH_SSH_ARGS
test_expect_success SEQ 'ssh works with pdsh -S and multiple targets' '
	for n in $(seq 1 24); do
		TEST_EXIT_CODE=$(random 254) &&
		FAILING_RANK=$(random $n) &&
		export PDSH_SSH_ARGS="-n%n -i$FAILING_RANK -e$TEST_EXIT_CODE"
		test_expect_code "$TEST_EXIT_CODE" pdsh -Rssh -S -wfoo[0-$n] command
	done
'
unset PDSH_SSH_ARGS

test_done

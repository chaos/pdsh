#!/bin/sh

test_description='test framework and pdsh basics'

if ! test -x ../src/pdsh/pdsh; then
   echo >&2 'Failed to find pdsh binary. Please run make.'
   exit 1
fi

. ${srcdir:-.}/test-lib.sh

###########################################################################
#
#  Tests of the framework. From git teststuite:
#
test_expect_success 'working success' ':'
test_expect_failure 'pretend known breakage' '
	false
'
test_expect_success 'pretend we have fixed a known breakage (run in sub test-lib)' "
    mkdir passing-todo &&
	(cd passing-todo &&
	cat >passing-todo.sh <<EOF &&
#!$SHELL_PATH

test_description='A passing TODO test

This is run in a sub test-lib so that we do not get incorrect passing
metrics
'

# Point to the t/test-lib.sh, which isn't in ../ as usual
TEST_DIRECTORY=\"$TEST_DIRECTORY\"
. \"\$TEST_DIRECTORY\"/test-lib.sh

test_expect_failure 'pretend we have fixed a known breakage' '
	:
'
test_done
EOF

chmod +x passing-todo.sh &&
./passing-todo.sh >out 2>err &&
! test -s err &&
sed -e 's/^> //' >expect <<EOF &&
> ok 1 - pretend we have fixed a known breakage # TODO known breakage
> # fixed 1 known breakage(s)
> # passed all 1 test(s)
> 1..1
EOF
   test_cmp expect out)
"

test_set_prereq HAVEIT
haveit=no
test_expect_success HAVEIT 'test runs if prerequisite is satisfied' '
    test_have_prereq HAVEIT &&
    haveit=yes
'

clean=no
test_expect_success 'tests clean up after themselves' '
    test_when_finished clean=yes
'

if test $clean != yes
then
	say "bug in test framework: basic cleanup command does not work reliably"
	exit 1
fi

test_expect_success 'tests clean up even on failures' "
    mkdir failing-cleanup &&
    (cd failing-cleanup &&
    cat >failing-cleanup.sh <<EOF &&
#!$SHELL_PATH

test_description='Failing tests with cleanup commands'

# Don't log these as failures by pretending we're running under TAP::Harness
HARNESS_ACTIVE=t
# Point to the t/test-lib.sh, which isn't in ../ as usual
TEST_DIRECTORY=\"$TEST_DIRECTORY\"
. \"\$TEST_DIRECTORY\"/test-lib.sh

test_expect_success 'tests clean up even after a failure' '
    touch clean-after-failure &&
    test_when_finished rm clean-after-failure &&
    (exit 1)
'

test_expect_success 'failure to clean up causes the test to fail' '
    test_when_finished \"(exit 2)\"
'

test_done
EOF
    chmod +x failing-cleanup.sh &&
    test_must_fail ./failing-cleanup.sh >out 2>err &&
    ! test -s err &&
    ! test -f \"trash directory.failing-cleanup/clean-after-failure\" &&
sed -e 's/Z$//' -e 's/^> //' >expect <<\EOF &&
> not ok - 1 tests clean up even after a failure
> #	Z
> #	    touch clean-after-failure &&
> #	    test_when_finished rm clean-after-failure &&
> #	    (exit 1)
> #	Z
> not ok - 2 failure to clean up causes the test to fail
> #	Z
> #	    test_when_finished \"(exit 2)\"
> #	Z
> # failed 2 among 2 test(s)
> 1..2
EOF
    test_cmp expect out)
"




###########################################################################
#
#  Basic pdsh functionality
#
test_expect_success 'pdsh runs' '
	pdsh -w foo -q | tail -1 | grep foo
'
test_expect_success 'pdsh -V works' '
    pdsh -V 2>&1 | grep -q ^pdsh
'
test_expect_success 'pdsh -L works' '
	pdsh -L 2>&1 | grep -q ^Module:
'
test_expect_success 'pdsh -h works' '
	pdsh -h 2>&1 | grep Usage:
'
test_expect_success 'rcmd/exec module is built' '
	test_have_prereq MOD_RCMD_EXEC && havit=yes
'
test_expect_success 'pdsh -N option works' '
	O1=$(pdsh -Rexec -w foo0 echo foo | sed "s/foo0: //")
	O2=$(pdsh -NRexec -w foo0 echo foo)
	if ! test "$O1" = "$O2"; then
	    say_color error "Error: -N output \"$O1\" != \"$O2\""
		false
    fi
'

test_expect_success LONGTESTS '-u option is functional' '
	run_timeout 5 pdsh -wfoo -Rexec -u 1 sleep 10 2>&1 \
            | grep -i "command timeout"
'

check_pdsh_option() {
	flag=$1; name=$2; value=$3;
	flagval=$value
	if test "$value" = "Yes" -o "$value" = "No"; then
		flagval=""
	fi
	echo "flag=$flag name='$name' value=$value flagval=$flagval"
	pdsh -$flag$flagval -w foo -q | grep -q "$name[ 	]*$value$"
}

check_pdsh_env_variable() {
	env_var=$1; name=$2; env_var_val=$3;
	echo "env_var=$env_var name='$name' env_var_val=$env_var_val"
	env $env_var=$env_var_val pdsh -w foo -q | grep -q "$name[ 	]*$env_var_val$"
}

test_expect_success '-f sets fanout' '
	check_pdsh_option f Fanout 8
'
test_expect_success '-l sets remote username' '
	check_pdsh_option l "Remote username" foouser
'
test_expect_success 'too long username fails gracefully' '
	i=0
	u="X"
	while [ $i -lt 512 ]; do
		u="${u}X"
		i=$((i+1))
	done
	pdsh -wfoo -l${u} -q 2>&1 | grep "exceeds max username length"
'
test_expect_success '-t sets connect timeout' '
	check_pdsh_option t "Connect timeout (secs)" 33
'
test_expect_success 'env PDSH_CONNECT_TIMEOUT sets connect timeout' '
	check_pdsh_env_variable PDSH_CONNECT_TIMEOUT "Connect timeout (secs)" 33
'
test_expect_success '-u sets command timeout' '
	check_pdsh_option u "Command timeout (secs)" 22
'
test_expect_success 'env PDSH_COMMAND_TIMEOUT sets command timeout' '
	check_pdsh_env_variable PDSH_COMMAND_TIMEOUT "Command timeout (secs)" 22
'
test_expect_success 'command timeout 0 by default' '
    pdsh -w foo -q | grep -q "Command timeout (secs)[ 	]*0$"
'
test_expect_success '-b enables batch mode' '
	check_pdsh_option b "one \^C will kill pdsh" Yes
'
test_expect_success 'pdsh -N option works' '
	O1=$(pdsh -Rexec -w foo0 echo foo | sed "s/foo0: //")
	O2=$(pdsh -NRexec -w foo0 echo foo)
	if ! test "$O1" = "$O2"; then
	    say_color error "Error: -N output \"$O1\" != \"$O2\""
		false
    fi
'

test_done

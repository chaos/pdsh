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

cleaner=no
test_expect_code 1 'tests clean up even after a failure' '
    test_when_finished cleaner=yes &&
    (exit 1)
'

if test $clean$cleaner != yesyes
then    
        say "bug in test framework: cleanup commands do not work reliably"
        exit 1
fi

test_expect_code 2 'failure to clean up causes the test to fail' '
    test_when_finished "(exit 2)"
'

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

run_timeout() {
	perl -e 'alarm shift @ARGV; exec @ARGV' "$@"
}
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

test_expect_success '-f sets fanout' '
	check_pdsh_option f Fanout 8
'
test_expect_success '-l sets remote username' '
	check_pdsh_option l "Remote username" foouser
'
test_expect_success '-t sets connect timeout' '
	check_pdsh_option t "Connect timeout (secs)" 33
'
test_expect_success '-u sets command timeout' '
	check_pdsh_option u "Command timeout (secs)" 22
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

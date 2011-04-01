#!/bin/sh

test_description='pdsh exec module tests'

. ${srcdir:-.}/test-lib.sh

if ! test_have_prereq MOD_RCMD_EXEC; then
	skip_all='skipping ssh tests, ssh module not available'
	test_done
fi

test_expect_success 'exec module works' '
	OUTPUT=$(pdsh -Rexec -w foo echo test_command)
	test "$OUTPUT" = "foo: test_command"
'
test_debug '
	echo Output: $OUTPUT
'
test_expect_success 'exec cmd substitution works ' '
	OUTPUT=$(pdsh -Rexec -w foo -l buser echo %u %h %n %%)
	test "$OUTPUT" = "foo: buser foo 0 %"
'
test_debug '
	echo Output: $OUTPUT
'

test_expect_success 'exec module works in interactive mode' '
	OUTPUT=$(echo echo test command | pdsh -R exec -w foo)
	echo "$OUTPUT" | grep "foo: test command"
'
test_debug '
	echo Output: $OUTPUT
'
test_expect_success 'exec cmd susbstitution works interactive mode' '
	OUTPUT=$(echo echo %u %h %n %% | pdsh -R exec -w foo -l auser)
	echo "$OUTPUT" | grep "foo: auser foo 0 %"
'
test_debug '
	echo Output: $OUTPUT
'
test_done

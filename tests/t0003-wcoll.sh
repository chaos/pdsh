#!/bin/sh

test_description='pdsh wcoll (working collective) argument processing'

. ${srcdir:-.}/test-lib.sh

#
#  Wrapper to check last line of output from pdsh -Qw
#   test_pdsh_wcoll <hostlist> <expected output> [<extra args>]
#
test_pdsh_wcoll() {
	OUTPUT=$(pdsh -Qw $1 $3 | tail -1)
	test_output_is_expected "$OUTPUT" "$2"
}

test_expect_success 'hostname range expansion works' '
	test_pdsh_wcoll "foo[0-3]" "foo0,foo1,foo2,foo3"
'
test_expect_success 'host range expansion does not strip leading zeros' '
	test_pdsh_wcoll "foo[00-02]" "foo00,foo01,foo02"
'
test_expect_success 'host range expansion handles mixed size suffixes' '
	test_pdsh_wcoll "foo[9-11]" "foo9,foo10,foo11"
'
test_expect_success 'host range expansion works with "," embedded in range' '
	test_pdsh_wcoll "foo[0-2,4]" "foo0,foo1,foo2,foo4"
'
test_expect_success 'host range expansion works with 2 sets of brackets' '
	test_pdsh_wcoll "foo[1-2]-[0-3]" \
		"foo1-0,foo1-1,foo1-2,foo1-3,foo2-0,foo2-1,foo2-2,foo2-3"
'
test_expect_success 'pdsh -x option works' '
	test_pdsh_wcoll "foo[9-11]" "foo9,foo11" "-x foo10"
'
test_expect_success 'pdsh -x option works with ranges' '
	test_pdsh_wcoll "foo[0-5]" "foo0,foo4,foo5" "-x foo[1-3]"
'
test_expect_success 'pdsh -x option works with ranges (gnats:118)' '
	test_pdsh_wcoll "foo[0-5]" "foo0,foo4,foo5" "-x foo[1-3]"
'
test_expect_success 'pdsh -x option works with non-numeric suffix (gnats:120)' '
	test_pdsh_wcoll "fooi,fooj,foo[0-5]" \
                        "foo0,foo1,foo3,foo4,foo5" \
                        "-x fooj,fooi,foo2"
'
test_expect_success 'pdsh -w- reads from stdin' '
	echo "foo1,foo2,foo3" | test_pdsh_wcoll "-" "foo1,foo2,foo3"
'
test_expect_success 'pdsh -w- can be used with other -w args' '
	echo "foo1,foo2,foo3" | test_pdsh_wcoll "-" "foo1,foo2,foo3,foo4" "-wfoo4" 
'
cat >wcoll <<EOF
foo1
foo2
foo3
EOF
test_expect_success 'WCOLL environment variable works' '
	test_output_is_expected "$(WCOLL=wcoll pdsh -q | tail -1)" "foo[1-3]"
'
cat >wcoll <<EOF
foo[9-11]
foo12
EOF
test_expect_success 'ranges can be embedded in wcoll files' '
	test_output_is_expected "$(WCOLL=wcoll pdsh -Q | tail -1)" \
                                "foo9,foo10,foo11,foo12"
'
test_expect_success '^file works' '
	test_pdsh_wcoll "^./wcoll" "foo9,foo10,foo11,foo12"
'
test_expect_success '-x ^file works' '
	test_pdsh_wcoll "foo[8-13]" "foo8,foo13" "-x^wcoll"
'
test_expect_success '^file works with other args' '
	test_pdsh_wcoll "foo[1-2],^./wcoll" "foo1,foo2,foo9,foo10,foo11,foo12"
'
cat >A <<EOF
foo1
foo2
EOF
cat >B <<EOF
foo7
foo8
EOF
test_expect_success 'Multiple ^file args' '
	test_pdsh_wcoll "^A,^B" "foo1,foo2,foo7,foo8"
'
test_expect_success 'Multiple -w^file' '
	test_pdsh_wcoll "^A" "foo1,foo2,foo7,foo8" "-w^B"
'
test_expect_success '-^file excludes hosts in file' '
	test_pdsh_wcoll "foo[8-12],-^./wcoll" "foo8"
'
test_expect_success '^file errors out if file doesnt exist' '
	! pdsh -w "^nosuchfile" -q 2>/dev/null &&
	pdsh -w "^nosuchfile" -q 2>&1 | grep -q "nosuchfile: No such file" 
'
test_expect_success 'host exclusion with "-" works' '
	test_pdsh_wcoll "foo[9-11],-foo10"  "foo9,foo11"
'
test_expect_success 'regex filtering works' '
	test_pdsh_wcoll "foo[0-20],/0$/" "foo0,foo10,foo20"
'
test_expect_success 'regex exclusion works' '
	test_pdsh_wcoll "foo[0-20],-/[1-9]$/" "foo0,foo10,foo20"
'
test_expect_success 'regex exclusion works from -x' '
	test_pdsh_wcoll "foo[0-20]" "foo0,foo10,foo20" "-x/[1-9]$/"
'
test_expect_success 'multiple -w options' '
	test_pdsh_wcoll "foo[0-20]" "foo0,foo10,foo20" "-w-/[1-9]$/" &&
	test_pdsh_wcoll "foo8" "foo8,foo9,foo10,foo11,foo12" "-w^wcoll" &&
	test_pdsh_wcoll "foo1,bar1" "foo1" "-w/^foo/"
'
test_done

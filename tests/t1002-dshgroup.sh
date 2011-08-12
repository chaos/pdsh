#!/bin/sh
#

test_description='dshgroup module'

. ${srcdir:-.}/test-lib.sh

if ! test_have_prereq MOD_MISC_DSHGROUP; then
	skip_all='skipping dshgroups tests, dshgroup module not available'
	test_done
fi

#
#  Ensure dshgroup module is loaded 
#
export PDSH_MISC_MODULES=dshgroup

mkdir -p .dsh/group/
cat >.dsh/group/groupA <<EOF
foo0
foo1
foo2
foo3
foo10
EOF
cat >.dsh/group/groupB <<EOF
foo5
foo4
foo3
EOF
cat >.dsh/group/groupAB <<EOF
#include groupA
#include groupB
foo8
EOF
cat >.dsh/group/groupA2 <<EOF
#include $(pwd)/.dsh/group/groupA
EOF

cat >.dsh/group/groupX <<EOF
#include groupY
foo1
EOF

cat >.dsh/group/groupY <<EOF
#include groupX
foo2
EOF

test_expect_success 'dshgroup options are active' '
	pdsh -h 2>&1 | grep -q "target hosts in dsh group"
'
test_expect_success 'dshgroup -g option works' '
	O=$(pdsh -g groupA -q | tail -1)
	test_output_is_expected "$O" "foo[0-3,10]"
'
test_expect_success 'dshgroup -g option works with more than one group' '
	O=$(pdsh -g groupA,groupB -q | tail -1)
	test_output_is_expected "$O" "foo[0-5,10]"
'
test_expect_success 'dshgroup -X option works' '
	O=$(pdsh -g groupA -X groupB -q | tail -1)
	test_output_is_expected "$O" "foo[0-2,10]"
'
test_expect_success 'dshgroup -X option works with -w' '
	O=$(pdsh -w foo[0-10] -X groupA -q | tail -1)
	test_output_is_expected "$O" "foo[4-9]"
'
test_expect_success 'dshgroup #include syntax works' '
    O=$(pdsh -g groupAB -q | tail -1)
	test_output_is_expected "$O" "foo[0-5,8,10]"
'
test_expect_success 'dshgroup #include syntax works with absolute paths' '
    O=$(pdsh -g groupA2 -q | tail -1)
	test_output_is_expected "$O" "foo[0-3,10]"
'
test_expect_success 'dshgroup #include syntax detects loops' '
    pdsh -g groupX -q 2>&1 | grep -q "warning:"
'

test_done

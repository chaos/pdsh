#!/bin/sh

test_description='genders module'

. ${srcdir:-.}/test-lib.sh

if ! test_have_prereq MOD_MISC_GENDERS; then
	skip_all='skipping genders tests, genders module not available'
	test_done
fi


#
#  Ensure genders module is loaded (i.e. same as -M genders)
#
export PDSH_MISC_MODULES=genders

cat >genders <<EOF
host[0-10] compute
EOF

cat >genders.A <<EOF
n[1-10] compute
n0      login,pdsh_all_skip
n[1-5]  os=debian
n[6-10] os=fedora
n[4-7]  foo
EOF

cat >genders.B <<EOF
n[1-10] altname=e%n
EOF

test_output_is_expected() {
	OUTPUT="$1"
	EXPECTED="$2"
	if ! test "$OUTPUT" = "$EXPECTED"; then
		say_color error "Error: Output \'$OUTPUT\' != \'$EXPECTED\'"
		false
	fi
}

test_expect_success 'genders -F works' '
	pdsh -F $(pwd)/genders -qa 
'
test_expect_failure 'genders -F works with relative paths' '
	pdsh -F ./genders.A -qa
'
test_expect_success 'genders -a skips pdsh_all_skip nodes' '
	OUTPUT=$(pdsh -F $(pwd)/genders.A -a -q | tail -1)
	test_output_is_expected "$OUTPUT" "n[1-10]"
'
test_expect_success 'genders -A selects all nodes in database' '
	OUTPUT=$(pdsh -F $(pwd)/genders.A -A -q | tail -1)
	test_output_is_expected "$OUTPUT" "n[0-10]"
'
test_expect_success 'genders selects altname by default' '
	OUTPUT=$(pdsh -F $(pwd)/genders.B -A -q | tail -1)
	test_output_is_expected "$OUTPUT" "en[1-10]"
'
test_expect_success 'genders -i option selects canonical name' '
	OUTPUT=$(pdsh -F $(pwd)/genders.B -A -i -q | tail -1)
	test_output_is_expected "$OUTPUT" "n[1-10]"
'
test_expect_success 'PDSH_GENDERS_FILE variable works' '
	OUTPUT=$(PDSH_GENDERS_FILE=$(pwd)/genders.A pdsh -aq | tail -1)
	test_output_is_expected "$OUTPUT"  "n[1-10]"
'

test_expect_failure 'PDSH_GENDERS_FILE variable works with relative paths' '
	OUTPUT=$(PDSH_GENDERS_FILE=./genders.A pdsh -aq | tail -1)
	test_output_is_expected "$OUTPUT" "n[1-10]"
'
test_expect_success 'PDSH_GENDERS_DIR variable works' '
	OUTPUT=$(PDSH_GENDERS_DIR=`pwd` pdsh -aq | tail -1)
	test_output_is_expected "$OUTPUT" "host[0-10]"
'

#
#  Load genders file from current dir by default
#
export PDSH_GENDERS_DIR=$(pwd)
test_expect_success 'pdsh -g option works' '
	OUTPUT=$(pdsh -F genders.A -g login -q | tail -1)
	test_output_is_expected "$OUTPUT" "n0"
'
test_expect_success 'pdsh -X option works' '
	OUTPUT=$(pdsh -F genders.A -AX login -q | tail -1)
	test_output_is_expected "$OUTPUT" "n[1-10]"
'

test_expect_success 'pdsh -g option supports var=val' '
	OUTPUT=$(pdsh -F genders.A -g os=debian -q | tail -1)
	test_output_is_expected "$OUTPUT" "n[1-5]"
'

test_expect_success 'pdsh -X option supports var=val' '
	OUTPUT=$(pdsh -F genders.A -AX os=debian -q | tail -1)
	test_output_is_expected "$OUTPUT" "n[0,6-10]"
'

test_expect_success 'pdsh -g option supports genders_query' '
	OUTPUT=$(pdsh -F genders.A -g "os=debian&&foo" -q | tail -1)
	test_output_is_expected "$OUTPUT" "n[4-5]"
'

test_expect_success 'pdsh -X option supports genders_query' '
	OUTPUT=$(pdsh -F genders.A -AX "os=fedora&&foo" -q | tail -1)
	test_output_is_expected "$OUTPUT" "n[0-5,8-10]"
'

test_expect_success 'pdsh -x excludes hosts selected by genders' '
	OUTPUT=$(pdsh -F genders.A -AX "os=fedora&&foo" -x n5 -q | tail -1)
	test_output_is_expected "$OUTPUT" "n[0-4,8-10]"
'

test_expect_success 'pdsh -w -host excludes hosts selected by genders' '
	OUTPUT=$(pdsh -F genders.A -A -w -n5 -q | tail -1)
	test_output_is_expected "$OUTPUT" "n[0-4,6-10]"
'

test_expect_success 'pdsh -w /regex/ filters hosts selected by genders' '
	OUTPUT=$(pdsh -F genders.A -A -w /n.*0$/ -q | tail -1)
	test_output_is_expected "$OUTPUT" "n[0,10]"
'

test_expect_success 'pdsh -w -/regex/ filters hosts selected by genders' '
	OUTPUT=$(pdsh -F genders.A -A -w -/n.*0$/ -q | tail -1)
	test_output_is_expected "$OUTPUT" "n[1-9]"
'
cat >genders.C <<EOF
node[0-10] compute,pdsh_rcmd_type=exec
EOF
test_expect_success MOD_RCMD_EXEC 'genders pdsh_rcmd_type attribute' '
    PDSH_RCMD_TYPE=ssh
	pdsh -S -Fgenders.C -A true
'

unset PDSH_GENDERS_DIR
unset PDSH_MISC_MODULES

test_done

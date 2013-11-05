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
n[1-10] foo,altname=e%n
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
test_expect_success 'genders -F works with relative paths' '
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
test_expect_success '-A returns altname by default' '
	OUTPUT=$(pdsh -F $(pwd)/genders.B -Aq | tail -1)
	test_output_is_expected "$OUTPUT" "en[1-10]"
'
test_expect_success '-g returns altname by default' '
	OUTPUT=$(pdsh -F./genders.B -gfoo -q | tail -1)
	test_output_is_expected "$OUTPUT" "en[1-10]"
'
test_expect_success 'PDSH_GENDERS_FILE variable works' '
	OUTPUT=$(PDSH_GENDERS_FILE=$(pwd)/genders.A pdsh -aq | tail -1)
	test_output_is_expected "$OUTPUT"  "n[1-10]"
'
test_expect_success 'PDSH_GENDERS_FILE variable works with relative paths' '
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
test_expect_success 'pdsh -g option works as filter' '
	OUTPUT=$(pdsh -F genders.A -w n[0-100] -g login -q | tail -1)
	test_output_is_expected "$OUTPUT" "n0"
'
test_expect_success 'Multiple -g option as filter are ORed together' '
	OUTPUT=$(pdsh -F genders.A -w n[0-100] -g login,blahblah -q | tail -1)
	test_output_is_expected "$OUTPUT" "n0"
'
test_expect_success 'pdsh -g as filter with invalid attr removes all hosts' '
	pdsh -F genders.A -w n[0-100] -g blahblah -q 2>&1 | \
		grep -q "no remote hosts specified"
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
test_expect_success 'missing genders file is not an error' '
	PDSH_GENDERS_FILE=doesnotexist
	if pdsh -w host[0-10] -q 2>&1 | grep -q error; then
	   say_color error "Error: Missing genders file causes error"
	   false
	fi
'
test_expect_success 'missing genders file with -F is an error' '
	if !pdsh -Fdoesnotexist -w host[0-10] -q 2>&1 | grep -q error; then
	   say_color error "Error: Missing genders file with -F doesnt cause error"
	   false
	fi
'

cat >genders.issue55 <<EOF
prefix1 skip_me
prefix-suffix1 no_skip
EOF
test_expect_success 'pdsh -X excludes more hosts than expected (Issue 55)' '
	OUTPUT=$(pdsh -F ./genders.issue55 -Aq -X skip_me | tail -1)
	test_output_is_expected "$OUTPUT" "prefix-suffix1"
'

cat >genders.is.slow <<EOF
foo[0-10000] test
foo[100-110] test2
EOF
test_expect_success 'genders query is slow' '
	run_timeout 1 pdsh -F ./genders.is.slow -g test -q
'
test_expect_success 'genders filter is slow' '
	run_timeout 1 pdsh -F ./genders.is.slow -w foo[0-10000] -g test2 -q
'

cat >genders.torture <<EOF
foo[1-1000] bar
foo[200-300] baz
foo700 pdsh_all_skip
EOF
results=(
'pdsh@<hostname>: no remote hosts specified'
'pdsh@<hostname>: no remote hosts specified'
'pdsh@<hostname>: no remote hosts specified'
'pdsh@<hostname>: no remote hosts specified'
'foo[1-1000]'
'foo[1-199,301-1000]'
'foo[1-49,101-1000]'
'foo[1-49,101-199,301-1000]'
'foo[31-2000]'
'foo[31-199,301-2000]'
'foo[31-49,101-2000]'
'foo[31-49,101-199,301-2000]'
'foo[31-1000]'
'foo[31-199,301-1000]'
'foo[31-49,101-1000]'
'foo[31-49,101-199,301-1000]'
'foo[1-699,701-1000]'
'foo[1-199,301-699,701-1000]'
'foo[1-49,101-699,701-1000]'
'foo[1-49,101-199,301-699,701-1000]'
'pdsh@<hostname>: Do not specify -a with -g'
'pdsh@<hostname>: Do not specify -a with -g'
'pdsh@<hostname>: Do not specify -a with -g'
'pdsh@<hostname>: Do not specify -a with -g'
'foo[31-699,701-2000]'
'foo[31-199,301-699,701-2000]'
'foo[31-49,101-699,701-2000]'
'foo[31-49,101-199,301-699,701-2000]'
'pdsh@<hostname>: Do not specify -a with -g'
'pdsh@<hostname>: Do not specify -a with -g'
'pdsh@<hostname>: Do not specify -a with -g'
'pdsh@<hostname>: Do not specify -a with -g'
'foo[1-1000]'
'foo[1-199,301-1000]'
'foo[1-49,101-1000]'
'foo[1-49,101-199,301-1000]'
'pdsh@<hostname>: Do not specify -a with -g'
'pdsh@<hostname>: Do not specify -a with -g'
'pdsh@<hostname>: Do not specify -a with -g'
'pdsh@<hostname>: Do not specify -a with -g'
'foo[31-2000]'
'foo[31-199,301-2000]'
'foo[31-49,101-2000]'
'foo[31-49,101-199,301-2000]'
'pdsh@<hostname>: Do not specify -a with -g'
'pdsh@<hostname>: Do not specify -a with -g'
'pdsh@<hostname>: Do not specify -a with -g'
'pdsh@<hostname>: Do not specify -a with -g'
'foo[1-699,701-1000]'
'foo[1-199,301-699,701-1000]'
'foo[1-49,101-699,701-1000]'
'foo[1-49,101-199,301-699,701-1000]'
'pdsh@<hostname>: Do not specify -a with -g'
'pdsh@<hostname>: Do not specify -a with -g'
'pdsh@<hostname>: Do not specify -a with -g'
'pdsh@<hostname>: Do not specify -a with -g'
'foo[31-699,701-2000]'
'foo[31-199,301-699,701-2000]'
'foo[31-49,101-699,701-2000]'
'foo[31-49,101-199,301-699,701-2000]'
'pdsh@<hostname>: Do not specify -a with -g'
'pdsh@<hostname>: Do not specify -a with -g'
'pdsh@<hostname>: Do not specify -a with -g'
'pdsh@<hostname>: Do not specify -a with -g'
)
let i=0
for A in "" "-A "; do
 for a in "" "-a "; do
  for w in "" "-w foo[31-2000] "; do
   for g in "" "-g bar "; do
    for x in "" "-x foo[50-100] "; do
     for X in "" "-X baz "; do
        test_expect_success "Py's genders test #$i ($A$a$w$g$x$X)" '
             OUTPUT=$(pdsh -F ./genders.torture -q $A$a$w$g$x$X 2>&1 |
                      tail -1 | sed "s/^\(pdsh@\)[^:]*/\1<hostname>/")
             test_output_is_expected "$OUTPUT" "${results[$i]}"
        '
        let i=i+1
     done
    done
   done
  done
 done
done

unset PDSH_GENDERS_DIR
unset PDSH_MISC_MODULES

test_done

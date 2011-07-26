#!/bin/sh

test_description='dshbak functionality' 

. ${srcdir:-.}/test-lib.sh

dshbak_test() {
	echo -e "$1" | dshbak -c  > output
	cat output \
           | while read line ; do
	       test "$line" = "$2" && echo ok
	   done | grep -q ok || (cat output >&2 && /bin/false)
}

dshbak_test_notok()
{
	touch ok
	echo -e "$1" \
		| dshbak -c \
		| while read line ; do
		    test "$line" = "$2" && rm ok
		  done
	rm ok && :
}

test_expect_success 'dshbak functionality' '
	cat >input <<EOF
foo0: bar
foo1: bar
EOF
	cat >output <<EOF
----------------
foo0
----------------
bar
----------------
foo1
----------------
bar
EOF
	dshbak <input >output2 &&
	diff output output2 &&
	rm input output* 
'


test_expect_success 'dshbak -c does not coalesce different length output' '
	dshbak_test_notok "
foo1: bar
foo2: bar
foo1: baz" "foo[1-2]"
'
test_expect_success 'dshbak -c properly compresses multi-digit suffixes' '
	dshbak_test "
foo8: bar
foo9: bar
foo10: bar
foo11: bar" "foo[8-11]"
'

test_expect_success 'dshbak -c properly compresses prefix with embedded numerals' '
	dshbak_test "
foo1x8: bar
foo1x9: bar
foo1x10: bar
foo1x11: bar" "foo1x[8-11]"
'
test_expect_success 'dshbak -c does not strip leading zeros' '
	dshbak_test "
foo01: bar
foo03: bar
foo02: bar
foo00: bar" "foo[00-03]"
'
test_expect_success 'dshbak -c does not coalesce different zero padding' '
	dshbak_test "
foo0: bar
foo03: bar
foo01: bar
foo2: bar" "foo[0,01,2,03]"
'
test_expect_success 'dshbak -c properly coalesces zero padding of "00"' '
	dshbak_test "
foo1: bar
foo01: bar
foo02: bar
foo3: bar
foo5: bar
foo00: bar" "foo[00-02,1,3,5]"
'
test_expect_success 'dshbak -c can detect suffixes' '
	dshbak_test "
foo1s: bar
foo01s: bar
foo02s: bar
foo3s: bar
foo5s: bar
foo00s: bar" "foo[00-02,1,3,5]s"
'
test_expect_failure 'dshbak -c can detect suffix with numeral' '
	dshbak_test "
foo1s0: bar
foo01s0: bar
foo02s0: bar
foo3s0: bar
foo5s0: bar
foo00s0: bar" "foo[00-02,1,3,5]s0"
'
test_expect_success 'issue 19: missing commas in dshbak header output' '
    dshbak_test "
foo1: bar
foo2: bar
foo5: bar
bar0: bar
bar1: bar" "bar[0-1],foo[1-2,5]"
'

test_expect_success 'dshbak properly joins 9,10' '
    dshbak_test "
foo1: bar
foo2: bar
foo3: bar
foo4: bar
foo5: bar
foo6: bar
foo7: bar
foo8: bar
foo9: bar
foo10: bar
foo11: bar" "foo[1-11]"
'

test_expect_success 'issue 33: dshbak does not coalesce 09,10' '
    dshbak_test "
foo01: bar
foo02: bar
foo03: bar
foo04: bar
foo05: bar
foo06: bar
foo07: bar
foo08: bar
foo09: bar
foo10: bar
foo11: bar" "foo[01-11]"
'

test_expect_success 'issue 33: dshbak does not coalesce 099,100' '
    dshbak_test "
foo090: bar
foo091: bar
foo092: bar
foo093: bar
foo094: bar
foo095: bar
foo096: bar
foo097: bar
foo098: bar
foo099: bar
foo100: bar
foo101: bar" "foo[090-101]"
'

cat >test_input <<EOF
test
input
file
 foo
bar  

EOF

test_expect_success 'dshbak -d functionality' '
  success=t
  mkdir test_output &&
  pdsh -w foo[0-10] -Rexec cat test_input | dshbak -d test_output &&
  for i in `seq 0 10`; do
	  diff -q test_input test_output/foo$i || success=f, break
  done &&
  test "$success" = "t" &&
  rm -rf test_output
'
test_expect_success 'dshbak -f functionality' '
  success=t
  pdsh -w foo[0-10] -Rexec cat test_input | dshbak -f -d test_output &&
  for i in `seq 0 10`; do
	  diff -q test_input test_output/foo$i || success=f, break
  done &&
  test "$success" = "t" &&
  rm -rf test_output
'
test_expect_success 'dshbak -f without -d fails' '
  dshbak -f </dev/null 2>&1 | grep "Option -f may only be used with -d"
'
test_expect_success 'dshbak -d fails when output dir does not exist' '
  dshbak -d does_not_exist </dev/null 2>&1 | \
     grep "Output directory does_not_exist does not exist"
'
test_expect_success SANITY 'dshbak -d fails gracefully for non-writable dir' '
  mkdir test_output &&
  chmod 500 test_output &&
  echo -e "foo0: bar" | dshbak -d test_output 2>&1 | tee logfile | \
     grep "Failed to open output file"  &&
  rm -rf test_output logfile || :
'

test_done

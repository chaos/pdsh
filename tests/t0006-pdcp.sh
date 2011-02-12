#!/bin/sh

test_description='pdcp specific tests'

if ! test -x ../src/pdsh/pdsh; then
   echo >&2 'Failed to find pdsh binary. Please run make.'
   exit 1
fi

. ${srcdir:-.}/test-lib.sh

###########################################################################
#
#  First set up pdcp link to pdsh
#

test_expect_success 'Creating pdcp link to pdsh binary' '
    ln -s $PDSH_BUILD_DIR/src/pdsh/pdsh pdcp
'

export PATH=.:$PATH

###########################################################################
#
#  Basic pdcp tests
#
test_expect_success 'pdcp runs' '
	pdcp -w foo -q * /tmp | tail -1 | grep foo
'
test_expect_success 'pdcp -V works' '
    pdcp -V 2>&1 | grep -q ^pdsh
'
test_expect_success 'pdcp -q works' '
	pdcp -q -w foo * /tmp 2>&1 | grep -q Infile
'
check_pdcp_option() {
	flag=$1; name=$2; value=$3;
	flagval=$value
	if test "$value" = "Yes" -o "$value" = "No"; then
		flagval=""
	fi
	echo "flag=$flag name='$name' value=$value flagval=$flagval"
	pdcp -$flag$flagval -w foo -q * /tmp | grep -q "$name[ 	]*$value$"
}

test_expect_success '-e sets remote program path' '
	check_pdcp_option e "Remote program path" /remote/pdcp
'
test_expect_success 'PDSH_REMOTE_PDCP_PATH sets remote program path' '
    tag="Remote program path"
	path="/xxx/pdcp"
	PDSH_REMOTE_PDCP_PATH=$path pdcp -w foo -q * /tmp | grep -q "$tag[ 	]*$path$" 
'
test_expect_success '-f sets fanout' '
	check_pdcp_option f Fanout 8
'
test_expect_success '-l sets remote username' '
	check_pdcp_option l "Remote username" foouser
'
test_expect_success '-t sets connect timeout' '
	check_pdcp_option t "Connect timeout (secs)" 33
'
test_expect_success '-u sets command timeout' '
	check_pdcp_option u "Command timeout (secs)" 22
'
test_expect_success 'command timeout 0 by default' '
    pdcp -w foo -q * /tmp | grep -q "Command timeout (secs)[ 	]*0$"
'
test_done

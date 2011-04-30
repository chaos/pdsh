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
test_expect_success 'Creating rpdcp link to pdsh binary' '
    ln -s $PDSH_BUILD_DIR/src/pdsh/pdsh rpdcp
'

export PATH=.:$PATH

###########################################################################
#
#  Basic pdcp tests
#
test_expect_success 'pdcp runs' '
	pdcp -w foo -q * /tmp | tail -1 | grep foo
'
test_expect_success 'rpdcp runs' '
	rpdcp -w foo -q * /tmp | tail -1 | grep foo
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

export T="$TEST_DIRECTORY/test-modules"

test_expect_success DYNAMIC_MODULES,NOTROOT 'Have pcptest rcmd module' '
	PDSH_MODULE_DIR=$T pdcp -L | grep -q pcptest
'

setup_host_dirs() {
	pdsh -w "$1" -Rexec mkdir %h
}

create_random_file() {
	name=${1-testfile}
	size=${2-1}
	dd if=/dev/urandom of=$name bs=1024 count=$size >/dev/null 2>&1
}

test_expect_success DYNAMIC_MODULES,NOTROOT 'pdcp basic functionality' '
    HOSTS="host[0-10]"
	setup_host_dirs "$HOSTS" &&
	test_when_finished "rm -rf host* testfile" &&
	create_random_file testfile 10 &&
	PDSH_MODULE_DIR=$T pdcp -Rpcptest -w "$HOSTS" testfile testfile &&
	pdsh -SRexec -w "$HOSTS" $GIT_TEST_CMP testfile %h/testfile
'
rm -rf host* testfile

test_expect_success DYNAMIC_MODULES,NOTROOT 'rpdcp basic functionality' '
	HOSTS="host[0-10]"
	setup_host_dirs "$HOSTS"
	test_when_finished "rm -rf host* t output" &&
	pdsh -Rexec -w "$HOSTS" dd if=/dev/urandom of=%h/t bs=1024 count=10 >/dev/null 2>&1 &&
	mkdir output &&
	PDSH_MODULE_DIR=$T rpdcp -Rpcptest -w "$HOSTS" t output/ &&
	pdsh -SRexec -w "$HOSTS" $GIT_TEST_CMP output/t.%h %h/t
'
test_expect_success DYNAMIC_MODULES,NOTROOT 'initialize directory tree' '
	mkdir tree &&
	(
	   cd tree &&
	   echo foo >foo &&
	   ln -s foo foo.link &&
	   mkdir -p dir/a/b/c/d/e &&
	   create_random_file dir/data 1024 &&
	   echo "deep dir" > dir/a/b/c/d/e/file &&
	   mkdir bar &&
	   echo "zzz" >bar/zzz &&
	   mkdir baz &&
	   echo "#!$SHELL_PATH" > baz/exec.sh &&
	   chmod +x baz/exec.sh &&
	   echo "write protected file" > dir/a/b/c/xw &&
	   chmod -w dir/a/b/c/xw
	)
'
test_expect_success DYNAMIC_MODULES,NOTROOT 'pdcp -r works' '
	HOSTS="host[0-10]"
	setup_host_dirs "$HOSTS" &&
	test_when_finished "rm -rf host*" &&
	PDSH_MODULE_DIR=$T pdcp -Rpcptest -w "$HOSTS" -r tree . &&
	pdsh -SRexec -w "$HOSTS" diff -r tree %h/tree >/dev/null &&
	pdsh -SRexec -w "$HOSTS" test -x tree/baz/exec.sh &&
	pdsh -SRexec -w "$HOSTS" test -h tree/foo.link &&
	pdsh -SRexec -w "$HOSTS" test ! -w dir/a/b/c/xw
'
test_expect_success DYNAMIC_MODULES,NOTROOT 'rpdcp -r works' '
	HOSTS="host[0-10]"
	setup_host_dirs "$HOSTS" &&
	test_when_finished "rm -rf host* output" &&
	pdsh -SRexec -w "$HOSTS" cp -r tree %h/ &&
	mkdir output &&
	PDSH_MODULE_DIR=$T rpdcp -Rpcptest -w "$HOSTS" -r tree output/ &&
	pdsh -SRexec -w "$HOSTS" diff -r tree output/tree.%h >/dev/null
'

test_done

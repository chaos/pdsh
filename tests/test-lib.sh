#!/bin/sh
#
# Copyright (c) 2005 Junio C Hamano
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see http://www.gnu.org/licenses/ .

# if --tee was passed, write the output not only to the terminal, but
# additionally to the file test-results/$BASENAME.out, too.

test_name=$(basename "$0" .sh)

case "$GIT_TEST_TEE_STARTED, $* " in
done,*)
	# do not redirect again
	;;
*' --tee '*|*' --va'*)
	mkdir -p test-results
	BASE=test-results/$test_name
	(GIT_TEST_TEE_STARTED=done ${SHELL-sh} "$0" "$@" 2>&1;
	 echo $? > $BASE.exit) | tee $BASE.out
	test "$(cat $BASE.exit)" = 0
	exit
	;;
esac

# Keep the original TERM for say_color
ORIGINAL_TERM=$TERM

# For repeatability, reset the environment to known value.
LANG=C
LC_ALL=C
PAGER=cat
TZ=UTC
TERM=dumb
export LANG LC_ALL PAGER TERM TZ
EDITOR=:
export EDITOR

#
#  If SHELL_PATH is not set, use a default of /bin/sh
#
SHELL_PATH=${SHELL_PATH:-/bin/sh}
export SHELL_PATH

unset VISUAL
#
#  Pdsh variables
#
unset WCOLL
unset PDSH_RCMD_TYPE
unset PDSH_MISC_MODULES
unset PDSH_MODULE_DIR
unset DSHPATH
unset FANOUT
unset PDSH_GENDERS_FILE
unset PDSH_GENDERS_DIR
unset PDSH_SSH_ARGS
unset PDSH_SSH_ARGS_APPEND
unset SLUMR_JOBID
unset PBS_JOBID

#
#  PDSH_BUILD_DIR and PDSH_SRC_DIR are set to build and src paths
#
if test -z "$PDSH_BUILD_DIR"; then
    if test -z "${builddir}"; then
	    PDSH_BUILD_DIR=$(pwd)/..
    else
	    PDSH_BUILD_DIR=$(cd ${builddir} && pwd)/..
	fi
	export PDSH_BUILD_DIR
fi
#
if test -z "$PDSH_SRC_DIR"; then
    if test -z "$srcdir"; then
	    PDSH_SRC_DIR=$(pwd)/..
    else
	    PDSH_SRC_DIR=$(cd ${srcdir} && pwd)/..
	fi
	export PDSH_SRC_DIR
fi

# Protect ourselves from common misconfiguration to export
# CDPATH into the environment
unset CDPATH

unset GREP_OPTIONS

# Convenience
#
# A regexp to match 5 and 40 hexdigits
_x05='[0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f]'
_x40="$_x05$_x05$_x05$_x05$_x05$_x05$_x05$_x05"

# Each test should start with something like this, after copyright notices:
#
# test_description='Description of this test...
# This test checks if command xyzzy does the right thing...
# '
# . ./test-lib.sh
[ "x$ORIGINAL_TERM" != "xdumb" ] && (
		TERM=$ORIGINAL_TERM &&
		export TERM &&
		[ -t 1 ] &&
		tput bold >/dev/null 2>&1 &&
		tput setaf 1 >/dev/null 2>&1 &&
		tput sgr0 >/dev/null 2>&1
	) &&
	color=t

while test "$#" -ne 0
do
	case "$1" in
	-d|--d|--de|--deb|--debu|--debug)
		debug=t; shift ;;
	-i|--i|--im|--imm|--imme|--immed|--immedi|--immedia|--immediat|--immediate)
		immediate=t; shift ;;
	-l|--l|--lo|--lon|--long|--long-|--long-t|--long-te|--long-tes|--long-test|--long-tests)
		PDSH_TEST_LONG=t; export PDSH_TEST_LONG; shift ;;
	-h|--h|--he|--hel|--help)
		help=t; shift ;;
	-v|--v|--ve|--ver|--verb|--verbo|--verbos|--verbose)
		verbose=t; shift ;;
	-q|--q|--qu|--qui|--quie|--quiet)
		# Ignore --quiet under a TAP::Harness. Saying how many tests
		# passed without the ok/not ok details is always an error.
		test -z "$HARNESS_ACTIVE" && quiet=t; shift ;;
	--with-dashes)
		with_dashes=t; shift ;;
	--no-color)
		color=; shift ;;
	--va|--val|--valg|--valgr|--valgri|--valgrin|--valgrind)
		valgrind=t; verbose=t; shift ;;
	--tee)
		shift ;; # was handled already
	--root=*)
		root=$(expr "z$1" : 'z[^=]*=\(.*\)')
		shift ;;
	*)
		echo "error: unknown test option '$1'" >&2; exit 1 ;;
	esac
done

if test -n "$color"; then
	say_color () {
		(
		TERM=$ORIGINAL_TERM
		export TERM
		case "$1" in
			error) tput bold; tput setaf 1;; # bold red
			skip)  tput bold; tput setaf 2;; # bold green
			pass)  tput setaf 2;;            # green
			info)  tput setaf 3;;            # brown
			*) test -n "$quiet" && return;;
		esac
		shift
		printf "%s" "$*"
		tput sgr0
		echo
		)
	}
else
	say_color() {
		test -z "$1" && test -n "$quiet" && return
		shift
		echo "$*"
	}
fi

error () {
	say_color error "error: $*"
	GIT_EXIT_OK=t
	exit 1
}

say () {
	say_color info "$*"
}

test "${test_description}" != "" ||
error "Test script did not set test_description."

if test "$help" = "t"
then
	echo "$test_description"
	exit 0
fi

exec 5>&1
if test "$verbose" = "t"
then
	exec 4>&2 3>&1
else
	exec 4>/dev/null 3>/dev/null
fi

test_failure=0
test_count=0
test_fixed=0
test_broken=0
test_success=0

test_external_has_tap=0

die () {
	code=$?
	if test -n "$GIT_EXIT_OK"
	then
		exit $code
	else
		echo >&5 "FATAL: Unexpected exit with code $code"
		exit 1
	fi
}

GIT_EXIT_OK=
trap 'die' EXIT

# The semantics of the editor variables are that of invoking
# sh -c "$EDITOR \"$@\"" files ...
#
# If our trash directory contains shell metacharacters, they will be
# interpreted if we just set $EDITOR directly, so do a little dance with
# environment variables to work around this.
#
# In particular, quoting isn't enough, as the path may contain the same quote
# that we're using.
test_set_editor () {
	FAKE_EDITOR="$1"
	export FAKE_EDITOR
	EDITOR='"$FAKE_EDITOR"'
	export EDITOR
}

test_decode_color () {
	sed	-e 's/.\[1m/<WHITE>/g' \
		-e 's/.\[31m/<RED>/g' \
		-e 's/.\[32m/<GREEN>/g' \
		-e 's/.\[33m/<YELLOW>/g' \
		-e 's/.\[34m/<BLUE>/g' \
		-e 's/.\[35m/<MAGENTA>/g' \
		-e 's/.\[36m/<CYAN>/g' \
		-e 's/.\[m/<RESET>/g'
}

q_to_nul () {
	perl -pe 'y/Q/\000/'
}

q_to_cr () {
	tr Q '\015'
}

q_to_tab () {
	tr Q '\011'
}

append_cr () {
	sed -e 's/$/Q/' | tr Q '\015'
}

remove_cr () {
	tr '\015' Q | sed -e 's/Q$//'
}

test_tick () {
	if test -z "${test_tick+set}"
	then
		test_tick=1112911993
	else
		test_tick=$(($test_tick + 60))
	fi
	GIT_COMMITTER_DATE="$test_tick -0700"
	GIT_AUTHOR_DATE="$test_tick -0700"
	export GIT_COMMITTER_DATE GIT_AUTHOR_DATE
}

# Use test_set_prereq to tell that a particular prerequisite is available.
# The prerequisite can later be checked for in two ways:
#
# - Explicitly using test_have_prereq.
#
# - Implicitly by specifying the prerequisite tag in the calls to
#   test_expect_{success,failure,code}.
#
# The single parameter is the prerequisite tag (a simple word, in all
# capital letters by convention).

test_set_prereq () {
	satisfied="$satisfied$1 "
}
satisfied=" "

test_have_prereq () {
	# prerequisites can be concatenated with ','
	save_IFS=$IFS
	IFS=,
	set -- $*
	IFS=$save_IFS

	total_prereq=0
	ok_prereq=0
	missing_prereq=

	for prerequisite
	do
		total_prereq=$(($total_prereq + 1))
		case $satisfied in
		*" $prerequisite "*)
			ok_prereq=$(($ok_prereq + 1))
			;;
		*)
			# Keep a list of missing prerequisites
			if test -z "$missing_prereq"
			then
				missing_prereq=$prerequisite
			else
				missing_prereq="$prerequisite,$missing_prereq"
			fi
		esac
	done

	test $total_prereq = $ok_prereq
}

# You are not expected to call test_ok_ and test_failure_ directly, use
# the text_expect_* functions instead.

test_ok_ () {
	test_success=$(($test_success + 1))
	say_color pass "ok $test_count - $@"
}

test_failure_ () {
	test_failure=$(($test_failure + 1))
	say_color error "not ok - $test_count $1"
	shift
	echo "$@" | sed -e 's/^/#	/'
	test "$immediate" = "" || { GIT_EXIT_OK=t; exit 1; }
}

test_known_broken_ok_ () {
	test_fixed=$(($test_fixed+1))
	say_color pass "ok $test_count - $@ # TODO known breakage"
}

test_known_broken_failure_ () {
	test_broken=$(($test_broken+1))
	say_color skip "not ok $test_count - $@ # TODO known breakage"
}

test_debug () {
	test "$debug" = "" || eval "$1"
}

test_run_ () {
	test_cleanup=:
	eval >&3 2>&4 "$1"
	eval_ret=$?
	eval >&3 2>&4 "$test_cleanup"
	if test "$verbose" = "t" && test -n "$HARNESS_ACTIVE"; then
		echo ""
	fi
	return 0
}

test_skip () {
	test_count=$(($test_count+1))
	to_skip=
	for skp in $PDSH_SKIP_TESTS
	do
		case $this_test.$test_count in
		$skp)
			to_skip=t
			break
		esac
	done
	if test -z "$to_skip" && test -n "$prereq" &&
	   ! test_have_prereq "$prereq"
	then
		to_skip=t
	fi
	case "$to_skip" in
	t)
		of_prereq=
		if test "$missing_prereq" != "$prereq"
		then
			of_prereq=" of $prereq"
		fi

		say_color skip >&3 "skipping test: $@"
		say_color skip "ok $test_count # skip $1 (missing $missing_prereq${of_prereq})"
		: true
		;;
	*)
		false
		;;
	esac
}

test_expect_failure () {
	test "$#" = 3 && { prereq=$1; shift; } || prereq=
	test "$#" = 2 ||
	error "bug in the test script: not 2 or 3 parameters to test-expect-failure"
	if ! test_skip "$@"
	then
		say >&3 "checking known breakage: $2"
		test_run_ "$2"
		if [ "$?" = 0 -a "$eval_ret" = 0 ]
		then
			test_known_broken_ok_ "$1"
		else
			test_known_broken_failure_ "$1"
		fi
	fi
	echo >&3 ""
}

test_expect_success () {
	test "$#" = 3 && { prereq=$1; shift; } || prereq=
	test "$#" = 2 ||
	error "bug in the test script: not 2 or 3 parameters to test-expect-success"
	if ! test_skip "$@"
	then
		say >&3 "expecting success: $2"
		test_run_ "$2"
		if [ "$?" = 0 -a "$eval_ret" = 0 ]
		then
			test_ok_ "$1"
		else
			test_failure_ "$@"
		fi
	fi
	echo >&3 ""
}


# Similar to test_must_fail and test_might_fail, but check that a
# given command exited with a given exit code. Meant to be used as:
#
#	test_expect_success 'Merge with d/f conflicts' '
#		test_expect_code 1 git merge "merge msg" B master
#	'

test_expect_code () {
	want_code=$1
	shift
	"$@"
	exit_code=$?
	if test $exit_code = $want_code
	then
		echo >&2 "test_expect_code: command exited with $exit_code: $*"
		return 0
	else
		echo >&2 "test_expect_code: command exited with $exit_code, we wanted $want_code $*"
		return 1
	fi
}

# test_external runs external test scripts that provide continuous
# test output about their progress, and succeeds/fails on
# zero/non-zero exit code.  It outputs the test output on stdout even
# in non-verbose mode, and announces the external script with "# run
# <n>: ..." before running it.  When providing relative paths, keep in
# mind that all scripts run in "trash directory".
# Usage: test_external description command arguments...
# Example: test_external 'Perl API' perl ../path/to/test.pl
test_external () {
	test "$#" = 4 && { prereq=$1; shift; } || prereq=
	test "$#" = 3 ||
	error >&5 "bug in the test script: not 3 or 4 parameters to test_external"
	descr="$1"
	shift
	if ! test_skip "$descr" "$@"
	then
		# Announce the script to reduce confusion about the
		# test output that follows.
		say_color "" "# run $test_count: $descr ($*)"
		# Export TEST_DIRECTORY, TRASH_DIRECTORY and GIT_TEST_LONG
		# to be able to use them in script
		export TEST_DIRECTORY TRASH_DIRECTORY GIT_TEST_LONG
		# Run command; redirect its stderr to &4 as in
		# test_run_, but keep its stdout on our stdout even in
		# non-verbose mode.
		"$@" 2>&4
		if [ "$?" = 0 ]
		then
			if test $test_external_has_tap -eq 0; then
				test_ok_ "$descr"
			else
				say_color "" "# test_external test $descr was ok"
				test_success=$(($test_success + 1))
			fi
		else
			if test $test_external_has_tap -eq 0; then
				test_failure_ "$descr" "$@"
			else
				say_color error "# test_external test $descr failed: $@"
				test_failure=$(($test_failure + 1))
			fi
		fi
	fi
}

# Like test_external, but in addition tests that the command generated
# no output on stderr.
test_external_without_stderr () {
	# The temporary file has no (and must have no) security
	# implications.
	tmp="$TMPDIR"; if [ -z "$tmp" ]; then tmp=/tmp; fi
	stderr="$tmp/git-external-stderr.$$.tmp"
	test_external "$@" 4> "$stderr"
	[ -f "$stderr" ] || error "Internal error: $stderr disappeared."
	descr="no stderr: $1"
	shift
	say >&3 "# expecting no stderr from previous command"
	if [ ! -s "$stderr" ]; then
		rm "$stderr"

		if test $test_external_has_tap -eq 0; then
			test_ok_ "$descr"
		else
			say_color "" "# test_external_without_stderr test $descr was ok"
			test_success=$(($test_success + 1))
		fi
	else
		if [ "$verbose" = t ]; then
			output=`echo; echo "# Stderr is:"; cat "$stderr"`
		else
			output=
		fi
		# rm first in case test_failure exits.
		rm "$stderr"
		if test $test_external_has_tap -eq 0; then
			test_failure_ "$descr" "$@" "$output"
		else
			say_color error "# test_external_without_stderr test $descr failed: $@: $output"
			test_failure=$(($test_failure + 1))
		fi
	fi
}

# debugging-friendly alternatives to "test [-f|-d|-e]"
# The commands test the existence or non-existence of $1. $2 can be
# given to provide a more precise diagnosis.
test_path_is_file () {
	if ! [ -f "$1" ]
	then
		echo "File $1 doesn't exist. $*"
		false
	fi
}

test_path_is_dir () {
	if ! [ -d "$1" ]
	then
		echo "Directory $1 doesn't exist. $*"
		false
	fi
}

test_path_is_missing () {
	if [ -e "$1" ]
	then
		echo "Path exists:"
		ls -ld "$1"
		if [ $# -ge 1 ]; then
			echo "$*"
		fi
		false
	fi
}


# This is not among top-level (test_expect_success | test_expect_failure)
# but is a prefix that can be used in the test script, like:
#
#	test_expect_success 'complain and die' '
#           do something &&
#           do something else &&
#	    test_must_fail git checkout ../outerspace
#	'
#
# Writing this as "! git checkout ../outerspace" is wrong, because
# the failure could be due to a segv.  We want a controlled failure.

test_must_fail () {
	"$@"
	exit_code=$?
	if test $exit_code = 0; then
		echo >&2 "test_must_fail: command succeeded: $*"
		return 1
	elif test $exit_code -gt 129 -a $exit_code -le 192; then
		echo >&2 "test_must_fail: died by signal: $*"
		return 1
	elif test $exit_code = 127; then
		echo >&2 "test_must_fail: command not found: $*"
		return 1
	fi
	return 0
}

# Similar to test_must_fail, but tolerates success, too.  This is
# meant to be used in contexts like:
#
#	test_expect_success 'some command works without configuration' '
#		test_might_fail git config --unset all.configuration &&
#		do something
#	'
#
# Writing "git config --unset all.configuration || :" would be wrong,
# because we want to notice if it fails due to segv.

test_might_fail () {
	"$@"
	exit_code=$?
	if test $exit_code -gt 129 -a $exit_code -le 192; then
		echo >&2 "test_might_fail: died by signal: $*"
		return 1
	elif test $exit_code = 127; then
		echo >&2 "test_might_fail: command not found: $*"
		return 1
	fi
	return 0
}

# test_cmp is a helper function to compare actual and expected output.
# You can use it like:
#
#	test_expect_success 'foo works' '
#		echo expected >expected &&
#		foo >actual &&
#		test_cmp expected actual
#	'
#
# This could be written as either "cmp" or "diff -u", but:
# - cmp's output is not nearly as easy to read as diff -u
# - not all diff versions understand "-u"

test_cmp() {
	$GIT_TEST_CMP "$@"
}

# This function can be used to schedule some commands to be run
# unconditionally at the end of the test to restore sanity:
#
#	test_expect_success 'test core.capslock' '
#		git config core.capslock true &&
#		test_when_finished "git config --unset core.capslock" &&
#		hello world
#	'
#
# That would be roughly equivalent to
#
#	test_expect_success 'test core.capslock' '
#		git config core.capslock true &&
#		hello world
#		git config --unset core.capslock
#	'
#
# except that the greeting and config --unset must both succeed for
# the test to pass.

test_when_finished () {
	test_cleanup="{ $*
		} && (exit \"\$eval_ret\"); eval_ret=\$?; $test_cleanup"
}

#
#  pdsh convenience functions
#
test_output_is_expected() {
	OUTPUT="$1"
	EXPECTED="$2"
	if ! test "$OUTPUT" = "$EXPECTED"; then
		say_color error "Error: Output \'$OUTPUT\' != \'$EXPECTED\'"
		false
	fi
}


# Most tests can use the created repository, but some may need to create more.
# Usage: test_create_repo <directory>
test_create_repo () {
        test "$#" = 1 ||
        error "bug in the test script: not 1 parameter to test-create-repo"
        repo="$1"
        mkdir -p "$repo"
}

test_done () {
	GIT_EXIT_OK=t

	if test -z "$HARNESS_ACTIVE"; then
		test_results_dir="$TEST_DIRECTORY/test-results"
		mkdir -p "$test_results_dir"
		test_results_path="$test_results_dir/$test_name-$$.counts"

		echo "total $test_count" >> $test_results_path
		echo "success $test_success" >> $test_results_path
		echo "fixed $test_fixed" >> $test_results_path
		echo "broken $test_broken" >> $test_results_path
		echo "failed $test_failure" >> $test_results_path
		echo "" >> $test_results_path
	fi

	if test "$test_fixed" != 0
	then
		say_color pass "# fixed $test_fixed known breakage(s)"
	fi
	if test "$test_broken" != 0
	then
		say_color error "# still have $test_broken known breakage(s)"
		msg="remaining $(($test_count-$test_broken)) test(s)"
	else
		msg="$test_count test(s)"
	fi
	case "$test_failure" in
	0)
		# Maybe print SKIP message
		[ -z "$skip_all" ] || skip_all=" # SKIP $skip_all"

		if test $test_external_has_tap -eq 0; then
			say_color pass "# passed all $msg"
			say "1..$test_count$skip_all"
		fi

		test -d "$remove_trash" &&
		cd "$(dirname "$remove_trash")" &&
		rm -rf "$(basename "$remove_trash")"

		exit 0 ;;

	*)
		if test $test_external_has_tap -eq 0; then
			say_color error "# failed $test_failure among $msg"
			say "1..$test_count"
		fi

		exit 1 ;;

	esac
}

# Test the binaries we have just built.  The tests are kept in
# t/ subdirectory and are run in 'trash directory' subdirectory.
if test -z "$TEST_DIRECTORY"
then
	# We allow tests to override this, in case they want to run tests
	# outside of t/, e.g. for running tests on the test library
	# itself.
	TEST_DIRECTORY=$(pwd)
fi

if test -n "$valgrind"
then
	make_symlink () {
		test -h "$2" &&
		test "$1" = "$(readlink "$2")" || {
			# be super paranoid
			if mkdir "$2".lock
			then
				rm -f "$2" &&
				ln -s "$1" "$2" &&
				rm -r "$2".lock
			else
				while test -d "$2".lock
				do
					say "Waiting for lock on $2."
					sleep 1
				done
			fi
		}
	}

	make_valgrind_symlink () {
		# handle only executables
		test -x "$1" || return

		base=$(basename "$1")
		symlink_target=$GIT_BUILD_DIR/$base
		# do not override scripts
		if test -x "$symlink_target" &&
		    test ! -d "$symlink_target" &&
		    test "#!" != "$(head -c 2 < "$symlink_target")"
		then
			symlink_target=../valgrind.sh
		fi
		case "$base" in
		*.sh|*.perl)
			symlink_target=../unprocessed-script
		esac
		# create the link, or replace it if it is out of date
		make_symlink "$symlink_target" "$GIT_VALGRIND/bin/$base" || exit
	}

	# override all executables in TEST_DIRECTORY/..
	GIT_VALGRIND=$TEST_DIRECTORY/valgrind
	mkdir -p "$GIT_VALGRIND"/bin
	make_valgrind_symlink $PDSH_BUILD_DIR/src/pdsh/pdsh
	IFS=$OLDIFS
	PATH=$GIT_VALGRIND/bin:$PATH
	export GIT_VALGRIND
elif test -n "$PDSH_TEST_INSTALLED" ; then
	PATH=$PDSH_TEST_INSTALLED:$PDSH_BUILD_DIR/src/pdsh:$PATH
	GIT_EXEC_PATH=${GIT_TEST_EXEC_PATH:-$GIT_EXEC_PATH}
else # normal case, use ../bin-wrappers only unless $with_dashes:
	pdsh_path=$PDSH_BUILD_DIR/src/pdsh
	dshbak_path=$PDSH_SRC_DIR/scripts
	test -n "$dshbak_path" && PATH="$dshbak_path:$PATH"
	test -n "$pdsh_path" && PATH="$pdsh_path:$PATH"
fi
export PATH

if test -z "$GIT_TEST_CMP"
then
	if test -n "$GIT_TEST_CMP_USE_COPIED_CONTEXT"
	then
		GIT_TEST_CMP="diff -c"
	else
		GIT_TEST_CMP="diff -u"
	fi
fi

test="trash-directory.$test_name"
test -n "$root" && test="$root/$test"
case "$test" in
/*) TRASH_DIRECTORY="$test" ;;
 *) TRASH_DIRECTORY="$TEST_DIRECTORY/$test" ;;
esac
test ! -z "$debug" || remove_trash=$TRASH_DIRECTORY
rm -fr "$test" || {
        GIT_EXIT_OK=t
        echo >&5 "FATAL: Cannot prepare test area"
        exit 1
}

test_create_repo "$test"

# Use -P to resolve symlinks in our working directory so that the cwd
# in subprocesses like git equals our $PWD (for pathname comparisons).
cd -P "$test" || exit 1

HOME=$(pwd)
export HOME

this_test=${0##*/}
this_test=${this_test%%-*}
for skp in $PDSH_SKIP_TESTS
do
	case "$this_test" in
	$skp)
		say_color skip >&3 "skipping test $this_test altogether"
		skip_all="skip all tests in $this_test"
		test_done
	esac
done

# Provide an implementation of the 'yes' utility
yes () {
	if test $# = 0
	then
		y=y
	else
		y="$*"
	fi

	while echo "$y"
	do
		:
	done
}

# Fix some commands on Windows
case $(uname -s) in
*MINGW*)
	# Windows has its own (incompatible) sort and find
	sort () {
		/usr/bin/sort "$@"
	}
	find () {
		/usr/bin/find "$@"
	}
	sum () {
		md5sum "$@"
	}
	# git sees Windows-style pwd
	pwd () {
		builtin pwd -W
	}
	# no POSIX permissions
	# backslashes in pathspec are converted to '/'
	# exec does not inherit the PID
	;;
*)
	test_set_prereq POSIXPERM
	test_set_prereq BSLASHPSPEC
	test_set_prereq EXECKEEPSPID
	;;
esac

test -z "$NO_PERL" && test_set_prereq PERL
test -z "$NO_PYTHON" && test_set_prereq PYTHON

# test whether the filesystem supports symbolic links
ln -s x y 2>/dev/null && test -h y 2>/dev/null && test_set_prereq SYMLINKS
rm -f y

# When the tests are run as root, permission tests will report that
# things are writable when they shouldn't be.
#
# Additionally, for pdsh, some tests require non-root, esp those
#  that use PDSH_MODULE_DIR, which doesn't work when run setuid or
#  as root
test -w / || test_set_prereq SANITY
test "$USER" = "root" || test_set_prereq NOTROOT

#  Set some prereqs for common commands
#
test "$(expr 9 - 2)" = "7" &&      test_set_prereq EXPR

#  Function to generate a random number
# 
random() { 
   R=$RANDOM
   if test -z "$R"; then
      if test -r /dev/urandom; then
         R=$(dd if=/dev/urandom count=1 2>/dev/null | cksum | cut -d' ' -f1)
      else
         R=$( (echo $$; ps; date +%s) 2>&1 | cksum | cut -d' ' -f1)
      fi
   fi
   if test -n "${1}"; then
      R=$(expr $R % $1)
   fi
   echo $R
}

#  Shell implementation of seq(1)
#
seq() {
	if [ $# -eq 1 ]; then
		i=1
		end=$1
	else
		i=$1
		end=$2
	fi
	while [ $i -le $end ]; do
		echo $i
		let i=$i+1
	done
}


#
# If the pdsh build directory owner and the pdsh binary have
#  different ownership, abort the test because pdsh will not
#  be able to load any modules, and almost no tests will work.
#
path_owner() { ls -ld $1 | awk '{print $3}'; }
pdsh_owner=$(path_owner $PDSH_BUILD_DIR/src/pdsh/pdsh)
builddir_owner=$(path_owner $PDSH_BUILD_DIR/src)
if test "$pdsh_owner" != "$builddir_owner"; then
  say_color error 'Build directory owner and pdsh binary owner are different'
  say_color error 'The testsuite will not work in this configuration'
  exit 1
fi

#
#  Set loaded modules as prereqs
#
for mod in $(pdsh -L 2>&1 | \
                 sed -n 's/^Module: *\(.*\)\/\(.*\)/\1_\2/p' | \
                 tr a-z A-Z); do
	test_set_prereq MOD_${mod}
done

if [ -n "$PDSH_TEST_LONG" ]; then
	test_set_prereq LONGTESTS
fi

if pdsh -V | head -1 | grep -qv +static-modules; then
	test_set_prereq DYNAMIC_MODULES
fi



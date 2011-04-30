#!/bin/sh
rm -rf test-results trash-directory*

SHELL_PATH="/bin/sh"

uname_s=$(uname -s)
case "${uname_s}" in
	AIX)  #  AIX /bin/sh is not functional with the testsuite
	      SHELL_PATH=$(which bash)
		  if [ -z "$SHELL_PATH" ]; then
			  echo "This is AIX and I can't find bash. HELP!"
		  fi
		  GIT_TEST_CMP="cmp"
		  export GIT_TEST_CMP SHELL_PATH
		  ;;
esac

code=0
for test in t[0-9]*.sh; do
	echo "*** $test ***"
	$SHELL_PATH ./$test
	if [ $? -ne 0 ]; then
		code=1
	fi
done

$SHELL_PATH aggregate-results.sh test-results/*

exit $code

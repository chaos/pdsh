#!/bin/sh
#
#  Run tests of the Moab reservation module if Moab is available and there are 
#   any currently active reservations.
#

test_description='moabrsv module'

. ${srcdir:-.}/test-lib.sh

if ! test_have_prereq MOD_MISC_MOABRSV; then
	skip_all='skipping moabrsv tests, moabrsv module not available'
	test_done
fi

if ! showres >/dev/null 2>&1; then
	skip_all='skipping moab tests, moab install not available'
	test_done
fi



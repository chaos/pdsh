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
	skip_all='skipping moab tests, showres not working'
	test_done
fi

if ! mrsvctl -q ALL >/dev/null 2>&1; then
	skip_all='skipping moab tests, mrsvctl not working'
	test_done
fi


#
#  Ensure moabrsv module is loaded
#
export PDSH_MISC_MODULES=moabrsv

RSVIDS=$(showres | egrep 'Job|User' | awk '{print $1}')
if [ -n "$RSVID" ]; then
   #
   # Use an existing reservation
   #
   RSVID=$(echo $RSVIDS | tr ' ' '\n' | head -1)
else
   #
   # Fail -- can't assume developer has permission to create a reservation
   #
   skip_all='skipping moab tests, unable to find a reservation'
   test_done
fi

#
#  Capture the nodes in reservation RSVID
#
NODES=$(mrsvctl -q --xml $RSVID | tr ' ' '\n' | grep AllocNodeList | sed 's/^AllocNodeList="//' | sed 's/"$//')

test_expect_success 'smoabrsv -r option works' '
	O=$(pdsh -r$RSVID -q | tail -1)
	if test "x$O" != "x$NODES"; then
	   say_color error "Error: pdsh -r$RSVID selected nodes $O expected $NOD
ES"
	   showres -n $RSVID 
	   false
        fi
'

test_expect_success 'moabrsv -r option handles illegal jobid gracefully' '
	pdsh -r garbage 2>&1 | grep -q "invalid setting"
'


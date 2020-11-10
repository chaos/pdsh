#!/bin/sh
#
#  Run tests of the SLURM module if slurm is available and there are 
#   any currently running jobs, or if we can start a job.
#

test_description='slurm module'

. ${srcdir:-.}/test-lib.sh

if ! test_have_prereq MOD_MISC_SLURM; then
	skip_all='skipping slurm tests, slurm module not available'
	test_done
fi

if ! squeue >/dev/null 2>&1; then
	skip_all='skipping slurm tests, slurm install not available'
	test_done
fi

export KILLJOBIDS=""
#
#  Create a batch job and return the jobid or FAILED on stdout
#
create_batch_job() {
	ID=$(printf '#!/bin/sh\nsleep 100\n'|sbatch "$@" |sed 's/Submitted batch job //')
	count=0
    while test "$(squeue -j $ID -ho %t)" != "R" && $count -lt 30; do
        sleep 1;
        $((count=count+1))
    done
	if test "$count" -ge 30; then
		echo FAILED
	else
		KILLJOBIDS="$KILLJOBIDS $JOBID"
		echo $ID
	fi
}

#
#  Ensure slurm module is loaded (i.e. same as -M genders)
#
export PDSH_MISC_MODULES=slurm

JOBIDS=$(squeue -ho %i -trunning)
if [ -n "$JOBIDS" ]; then
   #
   #  There are already running jobs we can use for testing
   #
   JOBID=$(echo $JOBIDS | tr ' ' '\n' | head -1)
else
   #
   #  Need to create our own job
   #   (Only run if long tests were requested)
   #
   if !	test_have_prereq LONGTESTS; then
      skip_all='skipping slurm tests, run with --long or PDSH_TEST_LONG'
	  test_done
   fi
   echo "Attempting to initiate slurm job" >&2
   JOBID=$(create_batch_job -N2)
   if test "$JOBID" = "FAILED"; then
      skip_all='skipping slurm tests, unable to run a job'
	  test_done
   fi
fi

#
#  Capture the nodes in job JOBID
#
NODES=$(squeue -ho %N -j $JOBID)

test_expect_success 'slurm -j option works' '
	O=$(pdsh -j$JOBID -q | tail -1)
	if test "x$O" != "x$NODES"; then
	   say_color error "Error: pdsh -j$JOBID selected nodes $O expected $NODES"
	   squeue -hj $JOBID 
	   false
    fi
'
test_expect_success 'slurm module reads SLURM_JOBID if no wcoll set' '
	O=$(SLURM_JOBID=$JOBID pdsh -q | tail -1)
	if test "x$O" != "x$NODES"; then
	   say_color error "Error: pdsh -j$JOBID selected nodes $O expected $NODES"
	   squeue -hj $JOBID 
	   false
    fi

'
test_expect_success 'slurm -j all option works' '
	O1=$(pdsh -j all -q | tail -1)
	O2=$(pdsh -j$(squeue -ho %i -trunning | tr " \n" ,,) -q | tail -1)
	if ! test "$O1" = "$O2"; then
	   say_color error "Error: pdsh -j all failed to select all allocated nodes"
	   say_color error "a: $O1"
	   say_color error "b: $O2"
	   false
	fi
'

test_expect_success LONGTESTS 'slurm -j all does not select completed jobs' '
	jobid=$(create_batch_job -N1) && test "$jobid" != "FAILED" &&
	node=$(squeue -ho%N -j $jobid) &&
	scancel $jobid &&
    while test "$(squeue -j "$jobid" -ho %t)" = "CG"; do sleep 0.2; done
	if pdsh -j all -Q | tail -1 | tr , "\n" | grep "^$node$"; then
	   if test "$(squeue -trunning -n$node -ho%t)" != "R"; then
	      say_color error "pdsh -j all selected node $node from completed job"
		  false
	   fi
	fi
'
test_expect_success 'slurm -j option handles illegal jobid gracefully' '
	pdsh -j garbage 2>&1 | grep -q "invalid setting"
'

test_expect_success 'slurm -P option works' '
	part=$(sinfo -ho %P | head -1)
	O1=$(sinfo -ho %N -p $part)
	O2=$(pdsh -P $part -q | tail -1)
        if test "x$O1" != "x$O2"; then
		say_color error "Error: pdsh -P $part selected nodes $O2 expected $O1"
		false
	fi
'

test_expect_success 'slurm -P works with -w' '
	part=$(sinfo -ho %P | head -1)
	O1=$(sinfo -ho %N -p $part)
	O2=$(pdsh -P $part -w foo,bar -q | tail -1)
	if test "foo,bar,x$O1" != "x$O2"; then
		say_color error "Error: pdsh -P $part -w foo,bar got $02 expected foo,bar,$O1"
	fi
'

test_expect_success 'slurm -C filters out nodes' '
	part=$(sinfo -ho %P | head -1)
	if pdsh -P $part -C featurethathopefullydoesntexist -q ; then
		say_color error "Error: pdsh -P $part -C featurethathopefullydoesntexist resulted in hosts"
	fi
'

#
#  Clean up:
#
echo "$KILLJOBIDS"
test -n "$KILLJOBIDS" && scancel $KILLJOBIDS

test_done

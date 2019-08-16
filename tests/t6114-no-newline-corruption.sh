#!/bin/sh

test_description='chaos/pdsh#114: pdsh garbles long output with no newline

Test that pdsh handles lots of data with no newline.'

. ${srcdir:-.}/test-lib.sh


perl -e 'print "a" x 1024' > 1K
perl -e 'print "a" x 8192' > 8K
perl -e 'print "a" x 8195' > 8K+
perl -e 'print "a" x 10000' > 10K

for i in 1K 8K 8K+ 10K; do
test_expect_success "pdsh does not garble $i with no newline" "
	pdsh -w foo -N -Rexec cat $i > output.${i} &&
	test_cmp ${i} output.${i}
"
done

for i in 1K 8K 8K+ 10K; do
test_expect_success "pdsh labels $i with no newline only once" '
	pdsh -w foo -Rexec cat $i | sed "s/foo/\n&\n/g" > labels.${i} &&
	test $(grep -c foo labels.${i}) -eq 1
'
done

test_done

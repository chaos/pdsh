#!/bin/sh
rm -rf test-results trash-directory*

for test in t[0-9]*.sh; do
	echo "*** $test ***"
	./$test
done

./aggregate-results.sh test-results/*

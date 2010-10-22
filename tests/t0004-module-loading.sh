#!/bin/sh

test_description='pdsh dynamic module support'

. ${srcdir:-.}/test-lib.sh

module_list () {
	pdsh -L "$EXTRA_PDSH_ARGS" 2>&1 | \
		perl -n -e '\
			chomp; ($k,$v) = split(/: */);
			$m = $v if ($k eq "Module");
			print "$m $v\n" if ($k eq "Active");'
}

loaded_modules() {
	module_list | awk '$2 == "yes" {print $1}'
}

conflicting_modules() {
	module_list | awk '$2 == "no" {print $1}'
}

module_is_active() {
	loaded_modules | while read m; do
		if [ "$m" = "$1" ]; then
			return 0
        fi 
	done
}

module_is_inactive() {
	conflicting_modules | while read m; do
		if [ "$m" = "$1" ]; then
			return 0
        fi 
	done
}

test_output_matches() {
	OUTPUT="$1"
	PATTERN="$2"
	if ! [[ "$OUTPUT" =~ "$PATTERN" ]]; then
		say_color error "Error: Didn't find pattern \"$PATTERN\""
		say_color info  "OUTPUT=$OUTPUT"
		false
	fi
}

unset EXTRA_PDSH_ARGS

test_expect_success 'PDSH_MODULE_DIR functionality' '
	PDSH_MODULE_DIR=$TEST_DIRECTORY/test-modules
	module_is_active A && module_is_active B
'

export PDSH_MODULE_DIR="$TEST_DIRECTORY/test-modules"

test_expect_success 'module A takes precedence over B' '
	module_is_active misc/A && module_is_inactive misc/B
'

test_expect_success 'pdsh -M B ativates module B' '
	EXTRA_PDSH_ARGS="-M B" 
	module_is_active misc/B && module_is_inactive misc/A
'
test_expect_success 'PDSH_MISC_MODULES option works' '
	PDSH_MISC_MODULES=B
	module_is_active misc/B && module_is_inactive misc/A
'
test_expect_success 'pdsh help string correctly displays options of loaded modules' '
	OUTPUT=$(pdsh -h 2>&1 | grep ^-a) &&
	test_output_matches "$OUTPUT" "Module A" &&
	OUTPUT=$(pdsh -M B -h 2>&1 | grep ^-a) &&
	test_output_matches "$OUTPUT" "Module B"
'
test_expect_success 'Loading conflicting module with -M causes error' '
	OUTPUT=$(pdsh -MA,B 2>&1 | grep Warning)
	test_output_matches "$OUTPUT" \
		"Failed to initialize requested module \"misc/B\""
'

test_expect_success 'Conflicting modules dont run init()' '
    PDSH_MODULE_DIR=$TEST_DIRECTORY/test-modules
    if pdsh -q 2>&1 | grep "B: in init"; then
	    say_color error "Error: init routine for module B run unexpectedly"
		false
	fi
'
test_expect_success 'Force loaded module runs init()' '
    PDSH_MODULE_DIR=$TEST_DIRECTORY/test-modules
    if ! pdsh -q -MB 2>&1 | grep "B: in init"; then
	    say_color error "Error: init routine for module B not run with -M B"
		false
    fi
'
test_expect_success 'New conflicting module does not run init() with -M' '
    PDSH_MODULE_DIR=$TEST_DIRECTORY/test-modules
    if pdsh -q -MB 2>&1 | grep "A: in init"; then
		say_color error "Error: A init routine run with -M B"
		false
	fi
'

test_done

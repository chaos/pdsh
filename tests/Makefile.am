
SUBDIRS = test-modules
CPPFLAGS = \
	-I $(top_srcdir)

T = $(wildcard $(srcdir)/t[0-9]*.sh)

TESTS = runtests.sh

EXTRA_DIST = \
    test-lib.sh \
    aggregate-results.sh \
    $(T)

clean-local:
	rm -fr trash-directory.* test-results .prove
	


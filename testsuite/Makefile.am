# $Id$

AUTOMAKE_OPTIONS = foreign dejagnu

RUNTESTDEFAULTFLAGS = --all --srcdir $(top_srcdir)/testsuite \
                      --objdir $(top_srcdir)
distclean-local:
	rm -f pdsh.log pdsh.sum
SUBDIRS = pdsh.test

# $Id$

AUTOMAKE_OPTIONS = foreign dejagnu

RUNTESTDEFAULTFLAGS = --all --srcdir $(top_srcdir)/testsuite \
                      --objdir $(top_builddir)
distclean-local:
	-rm -f *.log *.sum

SUBDIRS = pdsh.test

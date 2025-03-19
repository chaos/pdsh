##****************************************************************************
## $Id$
##****************************************************************************
## Process this file with automake to produce Makefile.in.
##****************************************************************************

include $(top_srcdir)/config/Make-inc.mk

ACLOCAL_AMFLAGS =              -I config
AUTOMAKE_OPTIONS =             foreign dist-bzip2
SUBDIRS =                      src tests doc scripts config

maintainer-clean-local:
	-(cd $(top_srcdir) && rm -rf autom4te.cache)
	-find . -name "Makefile.in" -exec rm {} \;

distclean-local::
	-rm -fr autom4te*.cache autoscan.*
	-rm -rf $(PACKAGE)-*

uninstall-local:
	@rmdir $(DESTDIR)$(pkglibdir); \
	 if [ -d  $(DESTDIR)$(pkglibdir) ]; then \
		echo >&2;\
		echo " Warning: The pdsh module directory $(DESTDIR)$(pkglibdir)" >&2;\
		echo " is not empty after 'make uninstall.' There may be modules" >&2;\
		echo " from a previous invocation of 'make install' that need" >&2;\
		echo " to be removed for a clean uninstall." >&2;\
		echo >&2;\
	 fi


MAINTAINERCLEANFILES = \
    aclocal.m4 \
    config.h.in \
    configure \
    stamp-h \
    stamp-h.in 

EXTRA_DIST = \
    DISCLAIMER.UC \
    DISCLAIMER.LLNS \
    pdsh.spec \
    README.KRB4 \
    README.modules \
    bootstrap 

# coverage

CODE_COVERAGE_LCOV_RMOPTS = \
	--ignore-errors unused

CODE_COVERAGE_IGNORE_PATTERN = \
        "/tests/*" \
        "/pdsh/cbuf.*" \
        "/common/hostlist.*" \
	"/usr/*"

CODE_COVERAGE_LCOV_OPTIONS =
@CODE_COVERAGE_RULES@

##****************************************************************************
## $Id$
##****************************************************************************
## Process this file with automake to produce Makefile.in.
##****************************************************************************

include $(top_srcdir)/config/Make-rpm.mk
include $(top_srcdir)/config/Make-inc.mk

ACLOCAL_AMFLAGS =              -I config
AUTOMAKE_OPTIONS =             foreign dist-bzip2
CONFIG_STATUS_DEPENDENCIES =   META
SUBDIRS =                      src testsuite doc etc scripts config


config.c: META config.h
	@(echo "char *pdsh_version = \"$(PDSH_VERSION)\";";\
	  echo "char *pdsh_module_dir = \"$(pkglibdir)\";"\
         )> config.c

testconfig.c: META config.h
	@(echo "char *pdsh_version = \"$(PDSH_VERSION)\";";\
          moddir=`cd $(top_srcdir)/modules && pwd`; \
	  echo "char *pdsh_module_dir = \"$$moddir\";"\
         )> testconfig.c

install-exec-hook:
	-mv $(DESTDIR)$(bindir)/pdsh.inst $(DESTDIR)$(bindir)/pdsh
	@echo "chown root $(DESTDIR)$(bindir)/pdsh"
	@chown root $(DESTDIR)$(bindir)/pdsh ||\
         echo "Unable to chown pdsh to root"
	@echo "chmod 4755 $(DESTDIR)$(bindir)/pdsh"
	@chmod 4755 $(DESTDIR)$(bindir)/pdsh ||\
         echo "Unable to set permissions on pdsh"
	@cp -p $(DESTDIR)$(bindir)/pdsh $(DESTDIR)$(bindir)/pdcp ||\
	 echo "Unable to copy pdsh to pdcp"

uninstall-local:
	$(RM) $(DESTDIR)$(bindir)/pdcp

maintainer-clean-local:
	-(cd $(top_srcdir) && rm -rf autom4te.cache)
	-find . -name "Makefile.in" -exec rm {} \;

distclean-local::
	-rm -fr autom4te*.cache autoscan.*
	-rm -rf $(PACKAGE)-*

MAINTAINERCLEANFILES = \
    aclocal.m4 \
    config.h.in \
    configure \
    stamp-h \
    stamp-h.in 

EXTRA_DIST = \
    DISCLAIMER META \
    pdsh.spec.in \
    README.KRB4 \
    README.QsNet \
    README.AIX \
    README.modules \
    bootstrap 

# $Id$

AUTOMAKE_OPTIONS =         foreign dist-bzip2

#
# WITH_STATIC_MODULES, the modules library must be built
# before pdsh is built, therefore the modules subdirectory
# is listed before the base directory.
#
# !WITH_STATIC_MODULES, some modules require libqsw to be built 
# from the base directory, therefore the modules subdirectory
# is listed after the base directory.
#
if WITH_STATIC_MODULES
SUBDIRS =                  net-toollib modules . testsuite 
else
SUBDIRS =                  net-toollib . modules testsuite 
endif

noinst_PROGRAMS=           pdsh 
bin_PROGRAMS =		   pdsh.inst
sbin_PROGRAMS =            @PROG_QSHD@ @PROG_MQSHD@

bin_SCRIPTS =              dshbak
EXTRA_PROGRAMS =           in.qshd in.mqshd
man_MANS =                 pdsh.1 pdcp.1 dshbak.1
noinst_LTLIBRARIES =       libqsw.la

if WITH_QSW
libqsw_la_SOURCES =        qswutil.c qswutil.h
endif

libqsw_la_LDFLAGS =        $(ELAN_LIBS)
EXTRA_libqsw_la_SOURCES =  qswutil.c qswutil.h

#
# ltdl.* only needed if loading modules dynamically
#
if !WITH_STATIC_MODULES
LTDL_SOURCES = ltdl.h ltdl.c
endif

pdsh_SOURCES =             $(COMMON_SOURCES) $(LTDL_SOURCES)
nodist_pdsh_SOURCES =      testconfig.c
pdsh_inst_SOURCES =        $(COMMON_SOURCES) $(LTDL_SOURCES)
nodist_pdsh_inst_SOURCES = config.c

if WITH_STATIC_MODULES
pdsh_LDADD =               $(READLINE_LIBS) 
# libmods.la must be before libqsw.la, due to linking dependencies
pdsh_LDFLAGS = 	           modules/libmods.la libqsw.la
else
pdsh_LDADD =               $(LIBADD_DL) $(READLINE_LIBS) 
pdsh_LDFLAGS =             -export-dynamic  
endif

pdsh_inst_LDADD =          $(pdsh_LDADD)
pdsh_inst_LDFLAGS =        $(pdsh_LDFLAGS)

COMMON_SOURCES = \
	main.c \
	opt.c opt.h \
	xmalloc.c xmalloc.h \
	xstring.c xstring.h \
	list.c list.h \
	wcoll.c wcoll.h \
	dsh.c dsh.h \
	err.c err.h \
	xpopen.c xpopen.h \
	hostlist.c hostlist.h \
	testcase.c \
	mod_rcmd.c mod_rcmd.h \
	mod.c mod.h \
	fd.c fd.h

in_qshd_SOURCES = \
	qshd.c \
	list.c list.h \
	xmalloc.c xmalloc.h \
	xstring.c xstring.h \
	err.c err.h \
	hostlist.c hostlist.h

in_qshd_LDADD = libqsw.la

in_mqshd_SOURCES = \
	mqshd.c \
	fd.c fd.h \
	xmalloc.c xmalloc.h \
	xstring.c xstring.h \
	err.c err.h \
	hostlist.c hostlist.h 

in_mqshd_LDADD = libqsw.la net-toollib/lib/libnettools.la

in_mqshd_LDFLAGS = $(MRSH_LIBS)

# Makefile rule for building rpm directly from CVS repository
include $(top_srcdir)/Make-rpm.mk

config.c: META
	@(echo "char *pdsh_version = \"$(PACKAGE)-$(VERSION)-$(RELEASE)\";";\
	  echo "char *pdsh_module_dir = \"$(pkglibdir)\";"\
         )> config.c

testconfig.c: META
	@(echo "char *pdsh_version = \"$(PACKAGE)-$(VERSION)-$(RELEASE)\";";\
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

MOSTLYCLEANFILES = \
    config.c \
    testconfig.c 

MAINTAINERCLEANFILES = \
    Makefile.in aclocal.m4 \
    config.h.in configure \
    stamp-h stamp-h.in \
    $(AUX_DIST)

# Extra files to distribute with `make dist'
EXTRA_DIST = \
    DISCLAIMER \
    README.KRB4 \
    README.QsNet\
    TODO \
    META \
    Make-rpm.mk \
    pdsh.spec.in \
    dshbak dshbak.1 \
    qshell.xinetd \
    mqshell.xinetd \
    autogen.sh    

# hack for automake version compatibility
# create auxdir/depcomp if it wasn't added by automake --add-missing
$(auxdir)/depcomp :
	touch $@

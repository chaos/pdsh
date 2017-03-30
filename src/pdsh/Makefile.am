##*****************************************************************************
## $Id$
##*****************************************************************************
## Process this file with automake to produce Makefile.in.
##*****************************************************************************

include $(top_srcdir)/config/Make-inc.mk

INCLUDES =                 -I$(top_srcdir)
noinst_PROGRAMS =          pdsh
bin_PROGRAMS =             pdsh.inst

if WITH_STATIC_MODULES
MODULE_LIBS =              $(top_builddir)/src/modules/libmods.la 
else
MODULE_FLAGS =             -export-dynamic $(AIX_PDSH_LDFLAGS)
endif

pdsh_LDADD =               $(READLINE_LIBS)
pdsh_LDFLAGS =             $(MODULE_LIBS) $(MODULE_FLAGS) \
                           $(top_builddir)/src/common/libcommon.la

pdsh_inst_LDADD =          $(pdsh_LDADD)
pdsh_inst_LDFLAGS =        $(pdsh_LDFLAGS)

pdsh_SOURCES =             $(PDSH_SOURCES)
pdsh_inst_SOURCES =        $(pdsh_SOURCES)
nodist_pdsh_SOURCES =      testconfig.c
nodist_pdsh_inst_SOURCES = config.c


PDSH_SOURCES = \
    main.c \
    dsh.c \
    dsh.h \
    mod.c \
    mod.h \
    rcmd.c \
    rcmd.h \
    opt.c \
    opt.h \
    privsep.c \
    privsep.h \
    pcp_server.c \
    pcp_server.h \
    pcp_client.c \
    pcp_client.h \
    testcase.c \
    wcoll.c \
    wcoll.h \
    cbuf.c \
    cbuf.h \
    xpopen.c \
    xpopen.h

config.c: $(top_srcdir)/META $(top_builddir)/config.h
	@(echo "char *pdsh_version = \"$(PDSH_VERSION_FULL)\";";\
          echo "char *pdsh_module_dir = \"$(pkglibdir)\";"\
         )> config.c

testconfig.c: $(top_srcdir)/META $(top_builddir)/config.h
	@(echo "char *pdsh_version = \"$(PDSH_VERSION_FULL)\";";\
          moddir=`cd $(top_builddir)/src/modules && pwd`; \
          echo "char *pdsh_module_dir = \"$$moddir\";"\
         )> testconfig.c

install-exec-hook:
	-mv $(DESTDIR)$(bindir)/pdsh.inst $(DESTDIR)$(bindir)/pdsh
	@echo "chown root $(DESTDIR)$(bindir)/pdsh"
	@chown root $(DESTDIR)$(bindir)/pdsh ||\
         echo "Unable to chown pdsh to root"
	@cp -p $(DESTDIR)$(bindir)/pdsh $(DESTDIR)$(bindir)/pdcp ||\
         echo "Unable to copy pdsh to pdcp"
	@cp -p $(DESTDIR)$(bindir)/pdsh $(DESTDIR)$(bindir)/rpdcp ||\
         echo "Unable to copy pdsh to rpdcp"

uninstall-local:
	$(RM) $(DESTDIR)$(bindir)/pdcp
	$(RM) $(DESTDIR)$(bindir)/rpdcp
	$(RM) $(DESTDIR)$(bindir)/pdsh

MOSTLYCLEANFILES = \
    config.c \
    testconfig.c

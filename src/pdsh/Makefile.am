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
LTDL_FILES =               ltdl.h ltdl.c
LTDL_LDADD =               $(LIBADD_DL)
endif

pdsh_LDADD =               $(READLINE_LIBS) $(LTDL_LDADD)
pdsh_LDFLAGS =             $(MODULE_LIBS) $(MODULE_FLAGS) \
                           $(top_builddir)/src/common/libcommon.la

pdsh_inst_LDADD =          $(pdsh_LDADD)
pdsh_inst_LDFLAGS =        $(pdsh_LDFLAGS)

pdsh_SOURCES =             $(PDSH_SOURCES) $(LTDL_FILES)
pdsh_inst_SOURCES =        $(pdsh_SOURCES)
nodist_pdsh_SOURCES =      testconfig.c
nodist_pdsh_inst_SOURCES = config.c


PDSH_SOURCES = \
    main.c \
    dsh.c \
    dsh.h \
    mod.c \
    mod.h \
    mod_rcmd.c \
    mod_rcmd.h \
    opt.c \
    opt.h \
    pcp_server.c \
    pcp_server.h \
    testcase.c \
    wcoll.c \
    wcoll.h \
    xpopen.c \
    xpopen.h

config.c: $(top_srcdir)/META $(top_builddir)/config.h
	@(echo "char *pdsh_version = \"$(PDSH_VERSION)\";";\
          echo "char *pdsh_module_dir = \"$(pkglibdir)\";"\
         )> config.c

testconfig.c: $(top_srcdir)/META $(top_builddir)/config.h
	@(echo "char *pdsh_version = \"$(PDSH_VERSION)\";";\
          moddir=`cd $(top_builddir)/src/modules && pwd`; \
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

MOSTLYCLEANFILES = \
    config.c \
    testconfig.c

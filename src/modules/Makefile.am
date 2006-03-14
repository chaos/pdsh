##*****************************************************************************
## $Id$
##*****************************************************************************
## Process this file with automake to produce Makefile.in.
##*****************************************************************************

include $(top_srcdir)/config/Make-inc.mk

AUTOMAKE_OPTIONS = foreign

INCLUDES =         -I$(top_srcdir)


if WITH_SSH
SSH_MODULE = sshcmd.la
endif

if WITH_QSHELL
QSHELL_MODULE = qcmd.la
endif

if WITH_LIBGENDERS
GENDERS_MODULE = genders.la
endif

if WITH_NODEATTR
NODEATTR_MODULE = nodeattr.la
endif

if WITH_NODEUPDOWN
NODEUPDOWN_MODULE = nodeupdown.la
endif

if WITH_RMS
RMS_MODULE = rms.la
endif

if WITH_MACHINES
MACHINES_MODULE = machines.la
endif

if WITH_SDR
SDR_MODULE = sdr.la
AM_CPPFLAGS = -DGENDERS_G_ONLY
endif

if WITH_MRSH
MRSH_MODULE = mcmd.la
endif

if WITH_MQSHELL
MQSHELL_MODULE = mqcmd.la
endif

if WITH_KRB4
KRB4_MODULE = k4cmd.la
endif

if WITH_RSH
RSH_MODULE = xrcmd.la
endif

if WITH_XCPU
XCPU_MODULE = xcpucmd.la
endif

if WITH_SLURM
SLURM_MODULE = slurm.la
endif

if WITH_DSHGROUP
DSHGROUP_MODULE = dshgroup.la
endif

if WITH_NETGROUP
NETGROUP_MODULE = netgroup.la
endif

if WITH_GNU_LD
VERSION_SCRIPT = \
	version.map
OTHER_FLAGS = \
	-Wl,--version-script=$(VERSION_SCRIPT)
endif

if WITH_STATIC_MODULES
noinst_LTLIBRARIES = \
	libmods.la

libmods_la_LIBADD = \
	$(LIBMODS_OBJS)

libmods_la_DEPENDENCIES = \
	$(LIBMODS_OBJS) 
else
pkglib_LTLIBRARIES = \
	$(RSH_MODULE) \
	$(XCPU_MODULE) \
	$(SSH_MODULE) \
	$(MRSH_MODULE) \
	$(MQSHELL_MODULE) \
	$(QSHELL_MODULE) \
	$(GENDERS_MODULE) \
	$(NODEATTR_MODULE) \
	$(NODEUPDOWN_MODULE) \
	$(RMS_MODULE) \
	$(MACHINES_MODULE) \
	$(SDR_MODULE) \
	$(KRB4_MODULE) \
	$(SLURM_MODULE) \
	$(DSHGROUP_MODULE) \
	$(NETGROUP_MODULE)

BUILT_SOURCES = \
	$(VERSION_SCRIPT)

MODULE_FLAGS = \
	-module -avoid-version $(OTHER_FLAGS)
endif

nodist_libmods_la_SOURCES = \
	$(top_builddir)/static_modules.h

EXTRA_libmods_la_SOURCES = \
	xrcmd.c \
	sshcmd.c \
	mcmd.c  \
	mqcmd.c \
	qcmd.c  \
	genders.c \
	nodeattr.c \
	nodeupdown.c \
	rms.c \
	machines.c \
	sdr.c \
	k4cmd.c \
	slurm.c \
	dshgroup.c \
	netgroup.c \
	xcpucmd.c

if WITH_QSW
QSNET_LIBS = \
	$(top_builddir)/src/qsnet/libqsw.la 
endif
	
libmods_la_LDFLAGS = \
	$(QSNET_LIBS) \
	$(MRSH_LIBS) \
	$(NODEUPDOWN_LIBS) \
	$(KRB_LIBS) \
	$(GENDERS_LIBS) \
	$(SLURM_LIBS)

xrcmd_la_SOURCES =        xrcmd.c 
xrcmd_la_LDFLAGS =        $(MODULE_FLAGS)
xcpucmd_la_SOURCES =      xcpucmd.c 
xcpucmd_la_LDFLAGS =      $(MODULE_FLAGS)
sshcmd_la_SOURCES =       sshcmd.c
sshcmd_la_LDFLAGS =       $(MODULE_FLAGS)
qcmd_la_SOURCES =         qcmd.c 
qcmd_la_LDFLAGS =         $(MODULE_FLAGS) 
qcmd_la_LIBADD =          $(top_builddir)/src/qsnet/libqsw.la
mcmd_la_SOURCES =         mcmd.c 
mcmd_la_LDFLAGS =         $(MODULE_FLAGS) $(MRSH_LIBS)
mqcmd_la_SOURCES =        mqcmd.c
mqcmd_la_LDFLAGS =        $(MODULE_FLAGS) $(MRSH_LIBS) 
mqcmd_la_LIBADD =         $(top_builddir)/src/qsnet/libqsw.la
k4cmd_la_SOURCES =        k4cmd.c 
k4cmd_la_LDFLAGS =        $(MODULE_FLAGS) $(KRB_LIBS)

machines_la_SOURCES =     machines.c 
machines_la_LDFLAGS =     $(MODULE_FLAGS)
genders_la_SOURCES =      genders.c 
genders_la_LDFLAGS =      $(MODULE_FLAGS) $(GENDERS_LIBS)
nodeattr_la_SOURCES =     nodeattr.c 
nodeattr_la_LDFLAGS =     $(MODULE_FLAGS)
nodeupdown_la_SOURCES =   nodeupdown.c 
nodeupdown_la_LDFLAGS =   $(MODULE_FLAGS) $(NODEUPDOWN_LIBS)
rms_la_SOURCES =          rms.c 
rms_la_LDFLAGS =          $(MODULE_FLAGS)
sdr_la_SOURCES =          sdr.c 
sdr_la_LDFLAGS =          $(MODULE_FLAGS)
slurm_la_SOURCES =        slurm.c 
slurm_la_LDFLAGS =        $(MODULE_FLAGS) $(SLURM_LIBS)
dshgroup_la_SOURCES =     dshgroup.c 
dshgroup_la_LDFLAGS =     $(MODULE_FLAGS) 
netgroup_la_SOURCES =     netgroup.c 
netgroup_la_LDFLAGS =     $(MODULE_FLAGS) 

$(VERSION_SCRIPT) : 
	(echo  "{ global:";                \
	 echo "    pdsh_module_info;";     \
	 echo "    pdsh_module_priority;"; \
	 echo "  local: *;";               \
	 echo "};") > $(VERSION_SCRIPT)

DISTCLEANFILES = \
	$(VERSION_SCRIPT)

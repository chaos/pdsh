# $Id$

INCLUDES =         -I $(top_srcdir)
AUTOMAKE_OPTIONS = foreign

if WITH_STATIC_MODULES
MODULE_FLAGS = 
else
MODULE_FLAGS =     -module -avoid-version
endif


libmods_la_SOURCES = static_modules.h
noinst_LTLIBRARIES = libmods.la

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
	k4cmd.c
 
libmods_la_LDFLAGS = \
	$(MRSH_LIBS) \
	$(NODEUPDOWN_LIBS) \
	$(KRB_LIBS) \
	$(GENDERS_LIBS)

if WITH_SSH
SSH_MODULE = sshcmd.la
endif

if WITH_ELAN
ELAN_MODULE = qcmd.la
endif

if WITH_LIBGENDERS
if WITH_SDR
GENDERS_MODULE = genders-g.la
else
GENDERS_MODULE = genders.la
endif
endif

if WITH_NODEATTR
if WITH_SDR
NODEATTR_MODULE = nodeattr-g.la
else 
NODEATTR_MODULE = nodeattr.la
endif
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
endif

if WITH_MRSH
MRSH_MODULE = mcmd.la
endif

if WITH_MQSH
MQSH_MODULE = mqcmd.la
endif

if WITH_KRB4
KRB4_MODULE = k4cmd.la
endif

if WITH_STATIC_MODULES
libmods_la_LIBADD = $(LIBMODS_OBJS)
libmods_la_DEPENDENCIES = $(LIBMODS_OBJS) 
else
pkglib_LTLIBRARIES = \
	xrcmd.la \
	$(SSH_MODULE) \
    $(MRSH_MODULE) \
    $(MQSH_MODULE) \
	$(ELAN_MODULE) \
	$(GENDERS_MODULE) \
	$(NODEATTR_MODULE) \
	$(NODEUPDOWN_MODULE) \
	$(RMS_MODULE) \
	$(MACHINES_MODULE) \
	$(SDR_MODULE) \
	$(KRB4_MODULE)
endif

xrcmd_la_SOURCES =        xrcmd.c
xrcmd_la_LDFLAGS =        $(MODULE_FLAGS)
sshcmd_la_SOURCES =       sshcmd.c
sshcmd_la_LDFLAGS =       $(MODULE_FLAGS)
qcmd_la_SOURCES =         qcmd.c
qcmd_la_LDFLAGS =         $(MODULE_FLAGS) 
qcmd_la_LIBADD =          $(top_builddir)/libqsw.la
mcmd_la_SOURCES =         mcmd.c
mcmd_la_LDFLAGS =         $(MODULE_FLAGS) $(MRSH_LIBS)
mqcmd_la_SOURCES =        mqcmd.c
mqcmd_la_LDFLAGS =        $(MODULE_FLAGS) $(MRSH_LIBS) 
mqcmd_la_LIBADD =         $(top_builddir)/libqsw.la
k4cmd_la_SOURCES =        k4cmd.c
k4cmd_la_LDFLAGS =        $(MODULE_FLAGS) $(KRB_LIBS)

machines_la_SOURCES =     machines.c
machines_la_LDFLAGS =     $(MODULE_FLAGS)
genders_la_SOURCES =      genders.c
genders_la_LDFLAGS =      $(MODULE_FLAGS) $(GENDERS_LIBS)
genders_g_la_SOURCES =    genders.c
genders_g_la_CFLAGS =     -DGENDERS_G_ONLY
genders_g_la_LDFLAGS =    $(MODULE_FLAGS)
nodeattr_la_SOURCES =     nodeattr.c
nodeattr_la_LDFLAGS =     $(MODULE_FLAGS)
nodeattr_g_la_SOURCES =   nodeattr.c
nodeattr_g_la_CFLAGS =    -DGENDERS_G_ONLY
nodeattr_g_la_LDFLAGS =   $(MODULE_FLAGS)
nodeupdown_la_SOURCES =   nodeupdown.c
nodeupdown_la_LDFLAGS =   $(MODULE_FLAGS) $(NODEUPDOWN_LIBS)
rms_la_SOURCES =          rms.c
rms_la_LDFLAGS =          $(MODULE_FLAGS)
sdr_la_SOURCES =          sdr.c
sdr_la_LDFLAGS =          $(MODULE_FLAGS)

force:
$(qcmd_la_LIBADD) : force
	@cd `dirname $@` && $(MAKE) `basename $@`

force:
$(mqcmd_la_LIBADD) : force
	@cd `dirname $@` && $(MAKE) `basename $@`

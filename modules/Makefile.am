# $Id$

INCLUDES =         -I $(top_srcdir)

if WITH_STATIC_MODULES

noinst_LTLIBRARIES = libmods.la

if WITH_ELAN
ELAN_FILE = qcmd.c
endif

if WITH_LIBGENDERS
GENDERS_FILE = genders.c
endif

if WITH_NODEATTR
NODEATTR_FILE = nodeattr.c
endif

if WITH_NODEUPDOWN
NODEUPDOWN_FILE = nodeupdown.c
endif

if WITH_RMS
RMS_FILE = rms.c
endif

if WITH_MACHINES
MACHINES_FILE = machines.c
endif

if WITH_SDR
SDR_FILE = sdr.c
endif

if WITH_MRSH
MRSH_FILE = mcmd.c
endif

if WITH_MQSH
MQSH_FILE = mqcmd.c
endif

if WITH_KRB4
KRB4_FILE = k4cmd.c
endif

libmods_la_SOURCES = \
	sshcmd.c \
	xrcmd.c \
	$(MRSH_FILE) \
	$(MQSH_FILE) \
	$(ELAN_FILE) \
	$(GENDERS_FILE) \
	$(NODEATTR_FILE) \
	$(NODEUPDOWN_FILE) \
	$(RMS_FILE) \
	$(MACHINES_FILE) \
	$(SDR_FILE) \
	$(KRB4_FILE)
 
libmods_la_LDFLAGS = \
	$(MRSH_LIBS) \
	$(NODEUPDOWN_LIBS) \
	$(KRB_LIBS) \
	$(GENDERS_LIBS)

else

AUTOMAKE_OPTIONS = foreign
MODULE_FLAGS =     -module -avoid-version

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

pkglib_LTLIBRARIES = \
	sshcmd.la \
	xrcmd.la \
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

xrcmd_la_SOURCES =        xrcmd.c
xrcmd_la_LDFLAGS =        $(MODULE_FLAGS)
sshcmd_la_SOURCES =       sshcmd.c
sshcmd_la_LDFLAGS =       $(MODULE_FLAGS)
qcmd_la_SOURCES =         qcmd.c
qcmd_la_LDFLAGS =         $(MODULE_FLAGS) $(top_builddir)/libqsw.la
mcmd_la_SOURCES =         mcmd.c
mcmd_la_LDFLAGS =         $(MODULE_FLAGS) $(MRSH_LIBS)
mqcmd_la_SOURCES =        mqcmd.c
mqcmd_la_LDFLAGS =        $(MODULE_FLAGS) $(MRSH_LIBS) $(top_builddir)/libqsw.la
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

endif

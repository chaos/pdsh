##*****************************************************************************
## $Id$
##*****************************************************************************
## Process this file with automake to produce Makefile.in.
##*****************************************************************************

include $(top_srcdir)/config/Make-inc.mk

INCLUDES = \
    -I$(top_srcdir)

noinst_LTLIBRARIES = \
    libqsw.la

EXTRA_PROGRAMS = \
    in.qshd \
    in.mqshd

sbin_PROGRAMS = \
    @PROG_QSHD@ \
    @PROG_MQSHD@

in_qshd_SOURCES = \
    qshell.c \
    qshell.h \
    qshd.c

in_qshd_LDADD = \
    $(top_builddir)/src/qsnet/libqsw.la \
    $(top_builddir)/src/common/libcommon.la  \
    $(QSHELL_LIBS)

in_mqshd_SOURCES = \
    qshell.c \
    qshell.h \
    mqshd.c

in_mqshd_LDADD = \
    $(top_builddir)/src/qsnet/libqsw.la \
    $(top_builddir)/src/common/libcommon.la \
    $(MRSH_LIBS) \
    $(QSHELL_LIBS)

libqsw_la_LDFLAGS = \
    $(ELAN_LIBS)

QSWSOURCES = \
    qswutil.c \
    qswutil.h \
    elanhosts.c \
    elanhosts.h

if WITH_QSW
libqsw_la_SOURCES = \
    $(QSWSOURCES)
endif
 
EXTRA_libqsw_la_SOURCES = \
    $(QSWSOURCES)


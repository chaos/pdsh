##*****************************************************************************
## $Id$
##*****************************************************************************
## Process this file with automake to produce Makefile.in.
##*****************************************************************************

include $(top_srcdir)/config/Make-inc.mk

AM_CPPFLAGS = \
    -I$(top_srcdir)

if WITH_QSW
noinst_LTLIBRARIES = \
	libqsw.la
endif

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
    $(ELAN_LIBS) \
    $(LIBADD_DL)

libqsw_la_SOURCES = \
    qswutil.c \
    qswutil.h 

# $Id$

AUTOMAKE_OPTIONS = foreign 

SUBDIRS = . testsuite

EXTRA_DIST = 		\
	auxdir		\
	dshbak dshbak.1 \
	README.KRB4 	\
	README.QsNet 	\
	TODO		\
	Make-rpm.mk     \
	META		\
	pdsh.spec.in

bin_PROGRAMS   = pdsh	
sbin_PROGRAMS  = @PROG_QSHD@
bin_SCRIPTS    = dshbak

EXTRA_PROGRAMS = qshd

man1_MANS      = pdsh.1 pdcp.1 dshbak.1

if WITH_SSH
SSHCMD_SRC = sshcmd.c 
endif
if WITH_ELAN
ELAN_SRC   = qcmd.c qswutil.c qswutil.h
endif
if WITH_KRB4
K4CMD_SRC  = k4cmd.c
endif

pdsh_SOURCES =			\
	main.c			\
	opt.c opt.h		\
	xmalloc.c xmalloc.h	\
	xstring.c xstring.h	\
	list.c list.h		\
	wcoll.c wcoll.h		\
	dsh.c dsh.h		\
	err.c err.h		\
	xpopen.c xpopen.h	\
	hostlist.c hostlist.h	\
	testcase.c		\
	xrcmd.c			\
	$(SSHCMD_SRC) $(ELAN_SRC) $(K4CMD_SRC)

# Extra sources that may appear in RCMD_SOURCES must be listed here:
EXTRA_pdsh_SOURCES =		\
	qswutil.c qswutil.h	\
	sshcmd.c		\
	k4cmd.c	

qshd_SOURCES =			\
	qshd.c 			\
	list.c list.h		\
	xmalloc.c xmalloc.h	\
	xstring.c xstring.h	\
	err.c err.h		\
	qswutil.c qswutil.h	\
	hostlist.c hostlist.h

MAINTAINERCLEANFILES = 		\
	Makefile.in aclocal.m4 config.guess \
	config.h.in config.sub configure install-sh \
	missing mkinstalldirs stamp-h.in

include $(top_srcdir)/Make-rpm.mk



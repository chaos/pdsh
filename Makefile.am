# $Id$

AUTOMAKE_OPTIONS = foreign 

SUBDIRS        = . testsuite

auxdir         = $(srcdir)/$(ac_aux_dir)
AUX_DIST       = $(auxdir)/config.guess $(auxdir)/config.sub \
		 $(auxdir)/install-sh $(auxdir)/missing \
		 $(auxdir)/mkinstalldirs $(auxdir)/depcomp

bin_PROGRAMS   = pdsh	
sbin_PROGRAMS  = @PROG_QSHD@
bin_SCRIPTS    = dshbak

EXTRA_PROGRAMS = in.qshd

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

# Extra sources included conditionally above must also be listed here
EXTRA_pdsh_SOURCES =		\
	qswutil.c qswutil.h	\
	qcmd.c			\
	sshcmd.c		\
	k4cmd.c	

in_qshd_SOURCES =		\
	qshd.c 			\
	list.c list.h		\
	xmalloc.c xmalloc.h	\
	xstring.c xstring.h	\
	err.c err.h		\
	qswutil.c qswutil.h	\
	hostlist.c hostlist.h

# Makefile rule for building rpm directly from CVS repository
include $(top_srcdir)/Make-rpm.mk

install-exec-hook:
	@echo "chown root $(DESTDIR)$(bindir)/pdsh"
	@chown root $(DESTDIR)$(bindir)/pdsh ||\
	   echo "Unable to chown pdsh to root"
	@echo "chmod 4755 $(DESTDIR)$(bindir)/pdsh"
	@chmod 4755 $(DESTDIR)$(bindir)/pdsh ||\
	   echo "Unable to set permissions on pdsh"
	$(LN_S) $(DESTDIR)$(bindir)/pdsh $(DESTDIR)$(bindir)/pdcp || \
	   cp -p $(DESTDIR)$(bindir)/pdsh $(DESTDIR)$(bindir)/pdcp || :

uninstall-local:
	$(RM) $(DESTDIR)$(bindir)/pdcp

#dist-hook:
#	(cd $(distdir) && mkdir $(auxdir)) || :
#	for file in $(AUX_DIST) ; do \
#	  cp $$file $(distdir)/$$file; \
#	done

maintainer-clean-local:
	(cd $(top_srcdir) && rm -rf autom4te.cache)

MAINTAINERCLEANFILES = 		\
	Makefile.in aclocal.m4  \
	config.h.in configure   \
	stamp-h stamp-h.in      \
	autom4te.cache 		\
	$(AUX_DIST)

# Extra files to distribute with `make dist'
EXTRA_DIST = 		\
	DISCLAIMER	\
	README.KRB4 	\
	README.QsNet 	\
	TODO		\
	META		\
	Make-rpm.mk     \
	pdsh.spec.in	\
	dshbak dshbak.1 \
	qshell.xinetd   \
	autogen.sh	\
	$(AUX_DIST)

# hack for automake version compatibility
# create auxdir/depcomp if it wasn't added by automake --add-missing
$(auxdir)/depcomp :
	touch $@

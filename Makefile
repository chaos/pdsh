#
# $Id$ 
#
# This makefile works for aix 4.3, linux (RedHat5.2/sparc), and 
# Digital Unix 4.0.
#
PACKAGE=	pdsh
VERSION=	1.5

OBJS= 		list.o xmalloc.o xstring.o dsh.o main.o opt.o wcoll.o \
		rcmd.o err.o pipecmd.o $(KRB_OBJS)
HDRS=		list.h xmalloc.h xstring.h dsh.h opt.h wcoll.h conf.h err.h

prefix=		/usr/local

top_srcdir=     .
mkinstalldirs=  $(SHELL) $(top_srcdir)/auxdir/mkinstalldirs

#
# if you wish to build with kerberos IV, uncomment these
# and set KRB4 to 1 in conf.h
#
#KRB_INC=	-I/usr/local/krb4/include
#KRB_LIB=	-L/usr/local/krb4/lib -lkrb -ldes
#KRB_OBJS=	k4cmd.o

# Solaris
#LIBS =	-lpthread -lgen -lnsl -lsocket
# AIX 4.2
#LIBS =	-lbsd_r $(KRB_LIB) -lpthreads
# Linux RH 6.2, AIX 4.3.x, OSF
LIBS = $(KRB_LIB) -lpthread

CC=		cc
CFLAGS=		-I. -g $(KRB_INC)
LDFLAGS=

all: pdsh

pdsh: $(OBJS) $(LIBOBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS) $(LIBS) $(LIBOBJS)

install:
	install -m 4755 -o root -g root pdsh 	$prefix/bin/pdsh
	install -m 4755 -o root -g root pdsh 	$prefix/bin/pdcp
	install -m 555  -o root -g root dshbak 	$prefix/bin/dshbak
	install -m 444  -o root -g root pdsh.1 	$prefix/man/man1/pdsh.1
	install -m 444  -o root -g root pdcp.1 	$prefix/man/man1/pdcp.1
	install -m 444  -o root -g root dshbak.1 $prefix/man/man1/dshbak.1

clean:
	rm -f $(OBJS) $(LIBOBJS) core a.out pdsh

$(OBJS): $(HDRS)


# DEVELOPER TARGETS - Borrowed from conman 0.1.3 (cdunlap@llnl.gov)

tar \
$(PACKAGE)-$(VERSION).tgz: VERSION
	@if test -z "$(PACKAGE)"; then echo "PACKAGE undefined"; exit 1; fi; \
	if test -z "$(VERSION)"; then echo "VERSION undefined"; exit 1; fi; \
	test -z "$$tag" && tag=`echo $(PACKAGE)-$(VERSION) | tr '.' '-'`; \
	ver=`cvs co -r $$tag -p $(PACKAGE)/VERSION 2>/dev/null | \
	  sed -ne 's/.*-\(.*\)/\1/p'`; \
	if test -z "$$ver"; then echo "VERSION ($$tag) undefined"; exit 1; fi; \
	tmp=$${TMPDIR-/tmp}/tmp-$(PACKAGE)-$$$$; \
	name=$(PACKAGE)-$$ver; \
	dir=$$tmp/$$name; \
	echo "creating $$name.tgz ($$tag)"; \
	$(mkinstalldirs) $$tmp >/dev/null; \
	cvs export -r $$tag -d $$dir $(PACKAGE) >/dev/null && \
	  (cd $$tmp; tar cf - $$name) | gzip -c9 > $$name.tgz; \
	rm -rf $$tmp

rpm: $(PACKAGE).spec $(PACKAGE)-$(VERSION).tgz
	@if test -z "$(PACKAGE)"; then echo "PACKAGE undefined"; exit 1; fi; \
	if test -z "$(VERSION)"; then echo "VERSION undefined"; exit 1; fi; \
	tmp=$${TMPDIR-/tmp}/tmp-$(PACKAGE)-$$$$; \
	test -f $(PACKAGE)-$(VERSION).tgz || exit 0; \
	for d in BUILD RPMS SOURCES SPECS SRPMS TMP; do \
	  $(mkinstalldirs) $$tmp/$$d >/dev/null; \
	done; \
	cp -p $(PACKAGE).spec $$tmp/SPECS; \
	cp -p $(PACKAGE)-$(VERSION).tgz $$tmp/SOURCES; \
	echo "creating $(PACKAGE)-$(VERSION) rpm"; \
	rpm --showrc | egrep "_(gpg|pgp)_name" >/dev/null && sign="--sign"; \
	rpm -ba --define "_tmppath $$tmp/TMP" --define "_topdir $$tmp" \
	  $$sign --quiet $$tmp/SPECS/$(PACKAGE).spec && \
	    cp -p $$tmp/RPMS/*/$(PACKAGE)-$(VERSION)*.*.rpm \
	      $$tmp/SRPMS/$(PACKAGE)-$(VERSION)*.src.rpm ./; \
	rm -rf $$tmp

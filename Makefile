#
# $Id$ 
#
# This makefile works for aix 4.3, linux (RedHat5.2/sparc), and 
# Digital Unix 4.0.
#
PACKAGE=pdsh
VERSION=1.5

OBJS= 	list.o xmalloc.o xstring.o dsh.o main.o opt.o wcoll.o \
	rcmd.o err.o pipecmd.o $(KRB_OBJS)
HDRS=	list.h xmalloc.h xstring.h dsh.h opt.h wcoll.h conf.h err.h
PROG=	pdsh
MODE=	4555
OWN=	root:root
LINKS=	pdcp
MAN1=	pdsh.1 dshbak.1 pdcp.1
MAN1DEST=/usr/local/man/man1
DEST=	/usr/local/bin
OTHER=	dshbak

mkinstalldirs=  $(SHELL) $(top_srcdir)/auxdir/mkinstalldirs
top_srcdir=     .
prefix=         /usr
exec_prefix=    ${prefix}
bindir=         ${exec_prefix}/bin
sbindir=        ${exec_prefix}/sbin
libdir=         ${exec_prefix}/lib


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

CC=	cc
CFLAGS=	-I. -g $(KRB_INC)
LDFLAGS=

all: $(PROG) $(MAN1)

$(PROG): $(OBJS) $(LIBOBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS) $(LIBS) $(LIBOBJS)

clean:
	rm -f $(OBJS) $(LIBOBJS) core a.out $(PROG)

install: $(PROG) $(MAN1) $(OTHER)
	cp $(PROG) $(DEST)/$(PROG)
	chmod $(MODE) $(DEST)/$(PROG)
	chown $(OWN) $(DEST)/$(PROG)
	for link in $(LINKS); do \
		ln -fs $(PROG) $(DEST)/$$link; \
	done
	for man in $(MAN1); do \
		cp $$man $(MAN1DEST)/$$man; \
		chmod 0444 $(MAN1DEST)/$$man; \
		chown $(OWN) $(MAN1DEST)/$$man; \
	done
	for file in $(OTHER); do \
		cp $$file $(DEST)/$$file; \
		chmod 555 $(DEST)/$$file; \
		chown $(OWN) $(DEST)/$$file; \
	done

$(OBJS): $(HDRS)


# DEVELOPER TARGETS

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

#
# $Id$ 
#
# This makefile works for aix 4.3, linux (RedHat5.2/sparc), and 
# Digital Unix 4.0.
#
PACKAGE=	pdsh
VERSION=	1.5
RELEASE=	2

OBJS= 		list.o xmalloc.o xstring.o dsh.o main.o opt.o wcoll.o \
		rcmd.o err.o pipecmd.o qcmd.o $(KRB_OBJS)
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
CFLAGS=		-Wall -I. -g $(KRB_INC)
LDFLAGS=

all: pdsh

pdsh: $(OBJS) $(LIBOBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS) $(LIBS) $(LIBOBJS)

qshd: qshd.o
	$(CC) -o $@ $< -lelan3 -lrmscall

install:
	install -m 4755 -o root -g root pdsh 	$(prefix)/bin/pdsh
	install -m 4755 -o root -g root pdsh 	$(prefix)/bin/pdcp
	install -m 555  -o root -g root dshbak $(prefix)/bin/dshbak
	install -m 444  -o root -g root pdsh.1 $(prefix)/man/man1/pdsh.1
	install -m 444  -o root -g root pdcp.1 $(prefix)/man/man1/pdcp.1
	install -m 444  -o root -g root dshbak.1 $(prefix)/man/man1/dshbak.1

clean:
	rm -f *.o core a.out pdsh qshd
	rm -f *.rpm *.tgz 

$(OBJS): $(HDRS)

include Make-rpm.mk

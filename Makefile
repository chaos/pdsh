#
# $Id$ 
#
PROJECT=	pdsh
VERSION=	1.5

PDSH_OBJS=	list.o xmalloc.o xstring.o err.o \
		dsh.o main.o opt.o wcoll.o xrcmd.o sshcmd.o \
		$(ELAN_OBJS) $(KRB_OBJS)

QSHD_OBJS=	list.o xmalloc.o xstring.o err.o qswutil.o qshd.o 

PREFIX=		/usr/local

#
# if you wish to build with kerberos IV, uncomment these
# and set KRB4 to 1 in conf.h
#
#KRB_INC=	-I/usr/local/krb4/include
#KRB_LIB=	-L/usr/local/krb4/lib -lkrb -ldes
#KRB_OBJS=	k4cmd.o

#
# Uncomment and set HAVE_ELAN3 to 1 in conf.h for Quadrics Elan support
#
ELAN_TARGS=	qshd
ELAN_OBJS=	qswutil.o qcmd.o
ELAN_LIB=	-lelan3 -lrmscall
ELAN_INC=

# Solaris
#LIBS =	-lpthread -lgen -lnsl -lsocket
# AIX 4.2
#LIBS =	-lbsd_r $(KRB_LIB) -lpthreads
# Linux RH 6.2, AIX 4.3.x, OSF
LIBS = $(KRB_LIB)  -lpthread $(ELAN_LIB)

CC=		gcc
CFLAGS=		-Wall -g $(KRB_INC) $(ELAN_INC)
LDFLAGS=

all: pdsh $(ELAN_TARGS)

pdsh: $(PDSH_OBJS)
	$(CC) -o $@ $(PDSH_OBJS) $(LDFLAGS) $(LIBS)

qshd: $(QSHD_OBJS)
	$(CC) -o $@ $(QSHD_OBJS) $(LIBS)

install:
	install -m 4755 -o root -g root pdsh 	$(PREFIX)/bin/pdsh
	install -m 4755 -o root -g root pdsh 	$(PREFIX)/bin/pdcp
	install -m 555  -o root -g root dshbak $(PREFIX)/bin/dshbak
	install -m 444  -o root -g root pdsh.1 $(PREFIX)/man/man1/pdsh.1
	install -m 444  -o root -g root pdcp.1 $(PREFIX)/man/man1/pdcp.1
	install -m 444  -o root -g root dshbak.1 $(PREFIX)/man/man1/dshbak.1

clean:
	rm -f *.o core a.out pdsh $(ELAN_TARGS)
	rm -f *.rpm *.tgz 

$(OBJS): $(HDRS)

include Make-rpm.mk

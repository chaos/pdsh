#
# $Id$ 
#
# This makefile works for aix 4.3, linux (RedHat5.2/sparc), and 
# Digital Unix 4.0.
#
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

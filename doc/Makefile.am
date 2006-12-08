##****************************************************************************
## $Id$
##****************************************************************************
## Process this file with automake to produce Makefile.in.
##****************************************************************************

man_MANS =        pdsh.1 pdcp.1 dshbak.1
EXTRA_DIST =      dshbak.1

install-data-local:
	$(INSTALL) -d -m 0755 "$(DESTDIR)$(mandir)/man1"
	ln -f -s pdcp.1 "$(DESTDIR)$(mandir)/man1/rpdcp.1"

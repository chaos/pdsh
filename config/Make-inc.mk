##*****************************************************************************
## $Id$
##*****************************************************************************
## Originally taken from Chris Dunlap's Munge project:
##   $MungeID: Make-inc.mk,v 1.2 2003/04/30 18:44:14 dun Exp$
##*****************************************************************************

# Dependencies to ensure requisite libraries are rebuilt
#

$(top_builddir)/src/common/libcommon.la \
$(top_builddit)/src/qsnet/libqsw.la \
$(top_builddit)/src/modules/libmods.la \
: force-dependency-check
	@cd `dirname $@` && make `basename $@`

force-dependency-check:

# Generic ``distclean'' hook.
#
# The double-colon allows this target to be defined multiple times,
#   thereby allowing a Makefile.am to include its own distclean-local hook.
#
distclean-local::
	-rm -f *~ \#* .\#* cscope*.out core *.core tags TAGS

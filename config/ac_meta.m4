##*****************************************************************************
## $Id$
##*****************************************************************************
#  AUTHOR:
#    Chris Dunlap <cdunlap@llnl.gov>
#
#  SYNOPSIS:
#    AC_META
#
#  DESCRIPTION:
#    Set PACKAGE and VERSION from the META file.
##*****************************************************************************

AC_DEFUN([AC_META],
[
  AC_MSG_CHECKING([metadata])

  PACKAGE="`perl -ne 'print,exit if s/^\s*NAME:\s*(\S*).*/\1/i' $srcdir/META`"
  AC_DEFINE_UNQUOTED([PACKAGE], ["$PACKAGE"], [Define the package name.])
  AC_SUBST([PACKAGE])

  VERSION="`perl -ne 'print,exit if s/^\s*VERSION:\s*(\S*).*/\1/i' $srcdir/META`"
  AC_DEFINE_UNQUOTED([VERSION], ["$VERSION"], [Define the package version.])
  AC_SUBST([VERSION])

  RELEASE="`perl -ne 'print,exit if s/^\s*RELEASE:\s*(\S*).*/\1/i' $srcdir/META`"
  AC_DEFINE_UNQUOTED([RELEASE], ["$RELEASE"], [Define the project's release.])
  AC_SUBST([RELEASE])

  AC_MSG_RESULT([yes])
])

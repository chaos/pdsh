##*****************************************************************************
## $Id$
##*****************************************************************************
#  AUTHOR:
#    Jim Garlick <garlick@llnl.gov>
#
#  SYNOPSIS:
#    AC_FANOUT
#
#  DESCRIPTION:
#    Adds support for the "--with-fanout=" configure script option to 
#    specify the default pdsh fanout.
#    
#
#  WARNINGS:
#    This macro must be placed after AC_PROG_CC or equivalent.
##*****************************************************************************

AC_DEFUN([AC_FANOUT],
[
  AC_MSG_CHECKING([for default fanout])
  AC_ARG_WITH([fanout],
    AC_HELP_STRING([--with-fanout=N], [Specify default fanout]),
    [ case "$withval" in
        no)  FANOUT=1 ;;
        *)   FANOUT=$withval ;;
      esac
    ]
  )
  AC_MSG_RESULT([${FANOUT=32}])
  AC_DEFINE_UNQUOTED(DFLT_FANOUT, $FANOUT, [Define to default pdsh fanout.])
])

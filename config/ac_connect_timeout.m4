##*****************************************************************************
## $Id$
##*****************************************************************************
#  AUTHOR:
#    Jim Garlick <garlick@llnl.gov>
#
#  SYNOPSIS:
#    AC_CONNECT_TIMEOUT
#
#  DESCRIPTION:
#    Adds support for the "--with-connect-timeout=" configure script 
#    option to specify the default pdsh connect timeout.
#    
#
#  WARNINGS:
#    This macro must be placed after AC_PROG_CC or equivalent.
##*****************************************************************************

AC_DEFUN([AC_CONNECT_TIMEOUT],
[
  AC_MSG_CHECKING([for default connect timeout])
  AC_ARG_WITH([timeout],
    AC_HELP_STRING([--with-timeout=N], 
	[Specify default connect timeout (secs)]),
    [ case "$withval" in
        no)  CONNECT_TIMEOUT=0 ;;
        *)   CONNECT_TIMEOUT=$withval ;;
      esac
    ]
  )
  AC_MSG_RESULT([${CONNECT_TIMEOUT=10}])
  AC_DEFINE_UNQUOTED(CONNECT_TIMEOUT, $CONNECT_TIMEOUT, 
		            [Define to default pdsh connect timeout.])
])

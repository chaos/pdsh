##*****************************************************************************
## $Id$
##*****************************************************************************
#  AUTHOR:
#    Mark Grondona <mgrondona@llnl.gov>
#
#  SYNOPSIS:
#    AC_MRSH
#
#  DESCRIPTION:
#    Checks for whether to include the standard "rsh" module. By default,
#    this module is included, but may be disabled by passing --without-rsh
#    to the configure script.
#
#  WARNINGS:
#    This macro must be placed after AC_PROG_CC or equivalent.
##*****************************************************************************

AC_DEFUN([AC_RSH],
[
  #
  # Check for whether to include the standard rsh module
  #
  AC_MSG_CHECKING([for whether to build rsh module])
  AC_ARG_WITH([rsh],
    AS_HELP_STRING([--without-rsh],[Do not include the standard rsh module]),
    [ case "$withval" in
        no)  ac_with_rsh=no ;;
        yes) ac_with_rsh=yes ;;
        *)   AC_MSG_RESULT([doh!])
             AC_MSG_ERROR([bad value "$withval" for --with-rsh]) ;;
      esac
    ]
  )
  AC_MSG_RESULT([${ac_with_rsh=yes}])
   
  if test "$ac_with_rsh" = "yes"; then
      AC_ADD_STATIC_MODULE("xrcmd")
  fi
])

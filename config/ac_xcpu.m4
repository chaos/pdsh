##*****************************************************************************
## $Id:$
##*****************************************************************************
#  AUTHOR:
#    Jim Garlick <garlick@llnl.gov>
#
#  SYNOPSIS:
#    AC_XCPU
#
#  DESCRIPTION:
#    Checks for xcpu
#
#  WARNINGS:
#    This macro must be placed after AC_PROG_CC or equivalent.
##*****************************************************************************

AC_DEFUN([AC_XCPU],
[
  #
  # Check for whether to include xcpu module
  #
  AC_MSG_CHECKING([for whether to build xcpu module])
  AC_ARG_WITH([xcpu],
    AC_HELP_STRING([--with-xcpu], [Build xcpu module]),
    [ case "$withval" in
        no)  ac_with_xcpu=no ;;
        yes) ac_with_xcpu=yes ;;
        *)   AC_MSG_RESULT([doh!])
             AC_MSG_ERROR([bad value "$withval" for --with-xcpu]) ;;
      esac
    ]
  )
  AC_MSG_RESULT([${ac_with_xcpu=no}])

  if test "$ac_with_xcpu" = "yes"; then
     ac_have_xcpu=yes
     AC_ADD_STATIC_MODULE("xcpucmd")
     AC_DEFINE([HAVE_XCPU], [1], [Define if you have XCPU.])
  fi

  AC_SUBST(HAVE_XCPU)

])

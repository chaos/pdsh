##*****************************************************************************
## $Id$
##*****************************************************************************
#  AUTHOR:
#    Jim Garlick <garlick@llnl.gov>
#
#  SYNOPSIS:
#    AC_RMSQUERY
#
#  DESCRIPTION:
#    Checks for whether to include rmsquery module
#
##*****************************************************************************

AC_DEFUN([AC_RMSQUERY],
[
  #
  # Check for whether to build rms module
  # 
  AC_MSG_CHECKING([for whether to build rms module])
  AC_ARG_WITH([rms],
    AC_HELP_STRING([--with-rms], 
      [support running pdsh under RMS allocation]),
      [ case "$withval" in
        yes) ac_with_rms=yes ;;
        no)  ac_with_rms=no ;;
        *)   AC_MSG_RESULT([doh!])
             AC_MSG_ERROR([bad value "$withval" for --with-rms]) ;;
      esac
    ]
  )
  AC_MSG_RESULT([${ac_with_rms=no}]) 

  if test "$ac_with_rms" = "yes"; then
    AC_PATH_PROG([RMSQUERY], [rmsquery], [], [/usr/bin:$PATH])
    if test -n "$RMSQUERY"; then
        ac_have_rmsquery=yes
        AC_ADD_STATIC_MODULE("rms")
        AC_DEFINE([HAVE_RMSQUERY], [1], [Define if you have rmsquery])
        AC_DEFINE_UNQUOTED(_PATH_RMSQUERY, "$RMSQUERY", [Path to rmsquery.])
    fi
  fi

  AC_SUBST(HAVE_RMSQUERY)
  AC_SUBST(RMSQUERY)      
])

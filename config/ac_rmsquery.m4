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
  # Check for whether to build nodeattr module
  #
  AC_MSG_CHECKING([for whether to build rms module])
  AC_ARG_WITH([rms],
    AC_HELP_STRING([--with-rms], 
      [support running pdsh under RMS allocation]),
    [ ac_with_rms=yes ]
  )
  AC_MSG_RESULT([${ac_with_rms=no}]) 

  if test "$ac_with_rms" = "yes"; then
    AC_PATH_PROG([RMSQUERY], [rmsquery], [], [/usr/bin:$PATH])
    if test -n "$RMSQUERY"; then
        AC_DEFINE_UNQUOTED(_PATH_RMSQUERY, "$RMSQUERY", [Path to rmsquery.])
    fi

  fi
])

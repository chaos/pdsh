##*****************************************************************************
## $Id$
##*****************************************************************************
#  AUTHOR:
#    Jim Garlick <garlick@llnl.gov>
#
#  SYNOPSIS:
#    AC_PAM
#
#  DESCRIPTION:
#    Checks for pam support. 
#
#  WARNINGS:
#    This macro must be placed after AC_PROG_CC or equivalent.
##*****************************************************************************

AC_DEFUN([AC_PAM],
[
  #
  # Default is to have pam built
  #
  AC_MSG_CHECKING([for whether to build with pam support])
  AC_ARG_WITH([pam],
    AC_HELP_STRING([--without-pam],
      [Do not build qshell/mqshell with pam support]),
    [ case "$withval" in
        no)  ac_with_pam=no ;;
        yes) ac_with_pam=yes ;;
        *)   AC_MSG_RESULT([doh!])
             AC_MSG_ERROR([bad value "$withval" for --with-pam]) ;;
      esac
    ]
  )
  AC_MSG_RESULT([${ac_with_pam=yes}])
])

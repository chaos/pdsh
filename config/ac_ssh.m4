##*****************************************************************************
## $Id$
##*****************************************************************************
#  AUTHOR:
#    Albert Chu
#
#  SYNOPSIS:
#    AC_SSH
#
#  DESCRIPTION:
#    Check if user wants to compile sshcmd 
#
#  WARNINGS:
#    This macro must be placed after AC_PROG_CC or equivalent.
##*****************************************************************************

AC_DEFUN([AC_SSH],
[
  #
  # Check for whether to include ssh module
  #
  AC_MSG_CHECKING([for whether to build ssh module])
  AC_ARG_WITH([ssh],
    AC_HELP_STRING([--with-ssh], [Build ssh module]),
    [ case "$withval" in
        no)  ac_with_ssh=no ;;
        yes) ac_with_ssh=yes ;;
        *)   AC_MSG_RESULT([doh!])
             AC_MSG_ERROR([bad value "$withval" for --with-ssh]) ;;
      esac
    ]
  )
  AC_MSG_RESULT([${ac_with_ssh=no}])
   
  if test "$ac_with_ssh" = "yes"; then
     ac_have_ssh=yes
     AC_ADD_STATIC_MODULE("sshcmd")
     AC_DEFINE([HAVE_SSH], [1], [Define if you have ssh.])
  fi

  AC_SUBST(HAVE_SSH)
])

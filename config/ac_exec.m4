##*****************************************************************************
## $Id$
##*****************************************************************************
#  AUTHOR:
#    Albert Chu
#
#  SYNOPSIS:
#    AC_EXEC
#
#  DESCRIPTION:
#    Check if user wants to compile execcmd
#
#  WARNINGS:
#    This macro must be placed after AC_PROG_CC or equivalent.
##*****************************************************************************

AC_DEFUN([AC_EXEC],
[
  #
  # Check for whether to include exec module
  #
  AC_MSG_CHECKING([for whether to build exec module])
  AC_ARG_WITH([exec],
    AC_HELP_STRING([--with-exec], [Build exec module]),
    [ case "$withval" in
        no)  ac_with_exec=no ;;
        yes) ac_with_exec=yes ;;
        *)   AC_MSG_RESULT([doh!])
             AC_MSG_ERROR([bad value "$withval" for --with-exec]) ;;
      esac
    ]
  )
  AC_MSG_RESULT([${ac_with_exec=yes}])
   
  if test "$ac_with_exec" = "yes"; then
     ac_have_exec=yes
     AC_ADD_STATIC_MODULE("execcmd")
  fi
])

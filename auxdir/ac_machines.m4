##*****************************************************************************
## $Id$
##*****************************************************************************
#  AUTHOR:
#    Jim Garlick <garlick@llnl.gov>
#
#  SYNOPSIS:
#    AC_MACHINES
#
#  DESCRIPTION:
#    Adds support for the "--with-machines=" configure script option to 
#    specify a flat file list of all nodes. 
#    
#
#  WARNINGS:
#    This macro must be placed after AC_PROG_CC or equivalent.
##*****************************************************************************

AC_DEFUN([AC_MACHINES],
[
  AC_MSG_CHECKING([for path to machines file])
  AC_ARG_WITH([machines],
    AC_HELP_STRING([--with-machines(=PATH)], [Specify a flat file list of all nodes]),
    [ case "$withval" in
        no)  ac_with_machines=no ;;
        yes) ac_with_machines=yes
             MACHINES="/etc/machines" ;;
        *)   ac_with_machines=yes
             MACHINES=$withval 
      esac
    ]
  )
  AC_MSG_RESULT([${ac_with_machines=no}])
  : ${ac_with_machines=no}
  if test "$ac_with_machines" = yes; then
        AC_DEFINE([HAVE_MACHINES], [1], [Define if you have machines])
	AC_DEFINE_UNQUOTED([_PATH_MACHINES], ["$MACHINES"], 
	        		             [Define to default machines file.])
  fi

  AC_SUBST(HAVE_MACHINES)
  AC_SUBST(MACHINES)
])

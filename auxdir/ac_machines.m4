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
    AC_HELP_STRING([--with-machines], [Specify a flat file list of all nodes]),
    [ case "$withval" in
        no)  ac_with_machines=no ;;
        yes) ac_with_machines=yes
             MACHINES_FILE="/etc/machines" ;;
        *)   ac_with_machines=yes
			 MACHINES_FILE=$withval 
      esac
    ]
  )
  AC_MSG_RESULT([${MACHINES_FILE=none}])
  : ${ac_with_machines=no}
  if test "$ac_with_machines" = yes; then
	  AC_DEFINE_UNQUOTED([_PATH_MACHINES], ["$MACHINES_FILE"], 
			             [Define to default machines file.])
  fi
  AC_SUBST(MACHINES_FILE)
])

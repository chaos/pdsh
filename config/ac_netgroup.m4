##*****************************************************************************
## $Id: ac_dshgroup.m4 771 2004-02-11 00:12:20Z grondo $
##*****************************************************************************
#  AUTHOR:
#    Mark Grondona <mgrondona@llnl.gov>
#
#  SYNOPSIS:
#    AC_NETGROUP
#
#  DESCRIPTION:
#    Checks for whether to include the standard dshgroup module.
#
#  WARNINGS:
#    This macro must be placed after AC_PROG_CC or equivalent.
##*****************************************************************************

AC_DEFUN([AC_NETGROUP],
[
  #
  # Check for whether to include the dshgroup module
  #
  AC_MSG_CHECKING([for whether to build netgroup module])
  AC_ARG_WITH([netgroup],
    AC_HELP_STRING([--with-netgroup], 
	  [Build netgroup module for netgroups support]),
    [ case "$withval" in
        no)  ac_with_netgroup=no ;;
        yes) ac_with_netgroup=yes ;;
        *)   AC_MSG_RESULT([doh!])
             AC_MSG_ERROR([bad value "$withval" for --with-netgroup]) ;;
      esac
    ]
  )
  AC_MSG_RESULT([${ac_with_netgroup=no}])
   
  if test "$ac_with_netgroup" = "yes"; then
      AC_ADD_STATIC_MODULE("netgroup")
  fi
])

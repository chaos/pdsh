##*****************************************************************************
## $Id: ac_dshgroup.m4 771 2004-02-11 00:12:20Z grondo $
##*****************************************************************************
#  AUTHOR:
#    Mark Grondona <mgrondona@llnl.gov>
#
#  SYNOPSIS:
#    AC_DSHGROUP
#
#  DESCRIPTION:
#    Checks for whether to include the standard dshgroup module.
#
#  WARNINGS:
#    This macro must be placed after AC_PROG_CC or equivalent.
##*****************************************************************************

AC_DEFUN([AC_DSHGROUP],
[
  #
  # Check for whether to include the dshgroup module
  #
  AC_MSG_CHECKING([for whether to build dshgroup module])
  AC_ARG_WITH([dshgroups],
    AC_HELP_STRING([--with-dshgroups], 
	  [Build dshgroup module for dsh group file support]),
    [ case "$withval" in
        no)  ac_with_dshgroup=no ;;
        yes) ac_with_dshgroup=yes ;;
        *)   AC_MSG_RESULT([doh!])
             AC_MSG_ERROR([bad value "$withval" for --with-dshgroups]) ;;
      esac
    ]
  )
  AC_MSG_RESULT([${ac_with_dshgroup=no}])
   
  if test "$ac_with_dshgroup" = "yes"; then
      AC_ADD_STATIC_MODULE("dshgroup")
  fi
])

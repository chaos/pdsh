##*****************************************************************************
## $Id$
##*****************************************************************************
#  AUTHOR:
#    Jim Garlick <garlick@llnl.gov>
#
#  SYNOPSIS:
#    AC_NODEUPDOWN
#
#  DESCRIPTION:
#    Checks for nodeupdown support. 
#
#  WARNINGS:
#    This macro must be placed after AC_PROG_CC or equivalent.
##*****************************************************************************

AC_DEFUN([AC_NODEUPDOWN],
[
  #
  # Check for whether to include libnodeupdown module
  #
  AC_MSG_CHECKING([for whether to build nodeupdown module])
  AC_ARG_WITH([nodeupdown],
    AC_HELP_STRING([--with-nodeupdown], [Build nodeupdown module]),
    [ case "$withval" in
        no)  ac_with_libnodeupdown=no ;;
        yes) ac_with_libnodeupdown=yes ;;
        *)   AC_MSG_RESULT([doh!])
             AC_MSG_ERROR([bad value "$withval" for --with-nodeupdown]) ;;
      esac
    ]
  )
  AC_MSG_RESULT([${ac_with_libnodeupdown=no}])
   
  if test "$ac_with_libnodeupdown" = "yes"; then
    AC_CHECK_LIB([nodeupdown], [nodeupdown_handle_create], 
                 [ac_have_libnodeupdown=yes], [])

   if test "$ac_have_libnodeupdown" != "yes" ; then
      AC_MSG_NOTICE([Cannot support nodeupdown without libnodeupdown])
   fi 

   if test "$ac_have_libnodeupdown" = "yes" ; then
      AC_ADD_STATIC_MODULE("nodeupdown")
      AC_DEFINE([HAVE_LIBNODEUPDOWN], [1], 
                [Define if you have libnodeupdown.])
      NODEUPDOWN_LIBS="-lnodeupdown"

      # Which nodeupdown API version do we have?
      AC_TRY_COMPILE(
           [#include <nodeupdown.h>],
           [nodeupdown_load_data(NULL, NULL, NULL, NULL, 0,0);],
           ac_nodeupdown_load_data_6=yes,
           ac_nodeupdown_load_data_6=no)
      AC_TRY_COMPILE(
           [#include <nodeupdown.h>],
           [nodeupdown_load_data(NULL, NULL, 0, 0, NULL);],
           ac_nodeupdown_load_data_5=yes,
           ac_nodeupdown_load_data_5=no)
      
      if test "$ac_nodeupdown_load_data_6" = "yes"; then
           AC_DEFINE(HAVE_NODEUPDOWN_LOAD_DATA_6, 1, 
                     [6 param nodeupdown_load_data])
      fi
      if test "$ac_nodeupdown_load_data_5" = "yes"; then
           AC_DEFINE(HAVE_NODEUPDOWN_LOAD_DATA_5, 1, 
                     [5 param nodeupdown_load_data])
      fi
    fi
  fi

  AC_SUBST(HAVE_LIBNODEUPDOWN)
  AC_SUBST(NODEUPDOWN_LIBS)
])

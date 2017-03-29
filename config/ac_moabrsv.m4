##*****************************************************************************
## $Id$
##*****************************************************************************
#  AUTHOR:
#    Troy Baer <troy@osc.edu>
#
#  SYNOPSIS:
#    AC_MOABRSV
#
#  DESCRIPTION:
#    Checks for whether to include moabrsv module
#
##*****************************************************************************

AC_DEFUN([AC_MOABRSV],
[
  #
  # Check for whether to build moabrsv module
  # 
  AC_MSG_CHECKING([for whether to build moabrsv module])
  AC_ARG_WITH([moabrsv],
    AC_HELP_STRING([--with-moabrsv], 
      [support running pdsh under MOABRSV allocation]),
      [ case "$withval" in
        yes) ac_with_moabrsv=yes ;;
        no)  ac_with_moabrsv=no ;;
        *)   AC_MSG_RESULT([doh!])
             AC_MSG_ERROR([bad value "$withval" for --with-moabrsv]) ;;
      esac
    ]
  )
  AC_MSG_RESULT([${ac_with_moabrsv=no}]) 

  if test "$ac_with_moabrsv" = "yes"; then
     PKG_CHECK_MODULES([LIBXML],[libxml-2.0],[ac_have_libxml2=yes])
	
     if test "$ac_have_libxml2" != "yes"; then
        AC_MSG_NOTICE([Cannot support moabrsv without libxml2.])
     else
        ac_have_moabrsv=yes
        AC_ADD_STATIC_MODULE("moabrsv")
        AC_DEFINE([HAVE_MOABRSV], [1], [Define if you have moabrsv.])
     fi
  fi

  AC_SUBST(HAVE_MOABRSV)
])

##*****************************************************************************
## $Id$
##*****************************************************************************
#  AUTHOR:
#    Jim Garlick <garlick@llnl.gov>
#
#  SYNOPSIS:
#    AC_GENDERS
#
#  DESCRIPTION:
#    Checks for genders support. 
#
#  WARNINGS:
#    This macro must be placed after AC_PROG_CC or equivalent.
##*****************************************************************************

AC_DEFUN([AC_GENDERS],
[
  #
  # Check for whether to include libgenders module
  #
  AC_MSG_CHECKING([for whether to build genders module])
  AC_ARG_WITH([genders],
    AC_HELP_STRING([--with-genders],
      [Build genders module for libgenders support]),
    [ case "$withval" in
        no)  ac_with_libgenders=no ;;
        yes) ac_with_libgenders=yes ;;
        *)   AC_MSG_RESULT([doh!])
             AC_MSG_ERROR([bad value "$withval" for --with-genders]) ;;
      esac
    ]
  )
  AC_MSG_RESULT([${ac_with_libgenders=no}])
    
  if test "$ac_with_libgenders" = "yes"; then
    AC_CHECK_LIB([genders], [genders_handle_create], 
                 [ac_have_libgenders=yes], [])

    if test "$ac_have_libgenders" = "yes"; then
      AC_ADD_STATIC_MODULE("genders")
      AC_DEFINE([HAVE_LIBGENDERS], [1], [Define if you have libgenders.])
      GENDERS_LIBS="-lgenders"
    fi

  fi

  AC_SUBST(HAVE_LIBGENDERS)
  AC_SUBST(GENDERS_LIBS)
])

##*****************************************************************************
## $Id$
##*****************************************************************************
#  AUTHOR:
#    Jim Garlick <garlick@llnl.gov>
#
#  SYNOPSIS:
#    AC_DMALLOC
#
#  DESCRIPTION:
#    Adds support for --with-dmalloc. 
#    
#
#  WARNINGS:
#    This macro must be placed after AC_PROG_CC or equivalent.
##*****************************************************************************

AC_DEFUN([AC_DMALLOC],
[
  AC_MSG_CHECKING([if malloc debugging is wanted])
  AC_ARG_WITH(dmalloc,
    AC_HELP_STRING([--with-dmalloc], [compile using Gray Watson's dmalloc]),
     [ case "$withval" in
        yes) ac_with_dmalloc=yes ;;
        no)  ac_with_dmalloc=no ;;
        *)   AC_MSG_RESULT([doh!])
             AC_MSG_ERROR([bad value "$withval" for --withdmalloc]) ;;
      esac
    ]
  )
  AC_MSG_RESULT([${ac_with_dmalloc=no}])

  if test "$ac_with_dmalloc" = "yes"; then
     AC_CHECK_LIB([dmalloc], [xmalloc],, 
       AC_MSG_ERROR([Cannot find libdmalloc!]))
     AC_DEFINE(WITH_DMALLOC, 1, 
              [Define if using dmalloc debugging malloc package.])
  fi
])

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
#    Checks for genders and nodeupdown support. For genders, tests for
#    both the nodeattr program and libgenders are performed. 
#
#  WARNINGS:
#    This macro must be placed after AC_PROG_CC or equivalent.
##*****************************************************************************

AC_DEFUN([AC_GENDERS],
[
  #
  # Check for whether to build nodeattr module
  #
  AC_MSG_CHECKING([for whether to build nodeattr module])
  AC_ARG_WITH([nodeattr],
    AC_HELP_STRING([--with-nodeattr(=PATH)], 
      [Build nodeattr module (PATH=program location)]),
    [ case "$withval" in
        no)  ac_with_nodeattr=no ;;
        yes) ac_with_nodeattr=yes ;;
        *)   ac_with_nodeattr=yes
             NODEATTR=$withval ;;
      esac
    ]
  )
  AC_MSG_RESULT([${ac_with_nodeattr=no}])

  if test "$ac_with_nodeattr" = "yes"; then
    # Note: AC_PATH_PROG will do nothing if NODEATTR already defined.
    AC_PATH_PROG([NODEATTR], [nodeattr], [], [/usr/bin:/admin/scripts:$PATH])

    if test -n "$NODEATTR"; then
      ac_have_nodeattr=yes
      AC_DEFINE_UNQUOTED(_PATH_NODEATTR, "$NODEATTR", [Path to nodeattr.])
      AC_DEFINE(HAVE_NODEATTR, 1, [Define if nodeattr program is available.])
    fi
  fi

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
      AC_DEFINE([HAVE_LIBGENDERS], [1], [Define if you have libgenders.])
      GENDERS_LIBS="-lgenders"
    fi

  fi


  #
  # Check for whether to include libnodeupdown module
  #
  AC_MSG_CHECKING([for whether to build nodeupdown module])
  AC_ARG_WITH([genders],
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
    if test "$ac_have_libnodeupdown" = "yes" ; then
      AC_DEFINE([HAVE_LIBNODEUPDOWN], [1], 
                [Define if you have libnodeupdown.])
      NODEUPDOWN_LIBS="-lnodeupdown"
    fi
  fi

  #
  # Do we have genders? (i.e. either nodeattr or libgenders)
  #
  if test "$ac_have_nodeattr" = "yes" ||
     test "$ac_have_genders"  = "yes" ; then
     AC_DEFINE([HAVE_GENDERS], [1], [Define if you have genders])
  fi
        
  AC_SUBST(HAVE_LIBGENDERS)
  AC_SUBST(GENDERS_LIBS)
  AC_SUBST(HAVE_LIBNODEUPDOWN)
  AC_SUBST(NODEUPDOWN_LIBS)
  AC_SUBST(HAVE_GENDERS)

])

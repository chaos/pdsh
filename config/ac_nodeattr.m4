##*****************************************************************************
## $Id$
##*****************************************************************************
#  AUTHOR:
#    Jim Garlick <garlick@llnl.gov>
#
#  SYNOPSIS:
#    AC_NODEATTR
#
#  DESCRIPTION:
#    Checks for nodeattr support.
#
#  WARNINGS:
#    This macro must be placed after AC_PROG_CC or equivalent.
##*****************************************************************************

AC_DEFUN([AC_NODEATTR],
[
  #
  # Check for whether to build nodeattr module
  #
  AC_MSG_CHECKING([for whether to build nodeattr module])
  AC_ARG_WITH([nodeattr],
    AS_HELP_STRING([--with-nodeattr(=PATH)],[Build nodeattr module (PATH=program location)]),
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

    # Newer versions of autoconf do not require the if statement
    # below, b/c AC_PATH_PROG will not execute if NODEATTR is already
    # defined.  But the if statement is necessary for older autoconfs.
    if test -z "$NODEATTR"; then
      AC_PATH_PROG([NODEATTR], [nodeattr], [], [/usr/bin:/admin/scripts:$PATH])
    fi

    if test -n "$NODEATTR"; then
      ac_have_nodeattr=yes
      AC_ADD_STATIC_MODULE("nodeattr")
      AC_DEFINE_UNQUOTED(_PATH_NODEATTR, "$NODEATTR", [Path to nodeattr.])
      AC_DEFINE([HAVE_NODEATTR], [1], [Define if nodeattr program is available.])
    fi
  fi

  AC_SUBST(HAVE_NODEATTR)
  AC_SUBST(NODEATTR)
])

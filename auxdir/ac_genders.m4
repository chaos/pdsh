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
  # Check for nodeattr program
  #
  AC_PATH_PROG([NODEATTR], [nodeattr], [], [/usr/bin:/admin/scripts:$PATH])
  if test -n "$NODEATTR"; then
      ac_have_nodeattr=yes
      AC_DEFINE_UNQUOTED(_PATH_NODEATTR, "$NODEATTR", [Path to nodeattr.])
	  AC_DEFINE(HAVE_NODEATTR, 1, [Define if nodeattr program is available.])
  fi
  AC_SUBST(NODEATTR)

  #
  # Check for libgenders
  #
  AC_CHECK_LIB([genders], [genders_handle_create], 
	           [ac_have_libgenders=yes], [])

  if test "$ac_have_libgenders" = "yes"; then
      AC_DEFINE([HAVE_LIBGENDERS], [1], [Define if you have libgenders.])
	  GENDERS_LIBS="-lgenders"

      #
      # Check for libnodeupdown
      #
	  AC_CHECK_LIB([nodeupdown], [nodeupdown_handle_create], 
			       [ac_have_libnodeupdown=yes], [])
	  if test "$ac_have_libnodeupdown" = "yes" ; then
	      AC_DEFINE([HAVE_LIBNODEUPDOWN], [1], 
				    [Define if you have libnodeupdown.])
		  NODEUPDOWN_LIBS="-lnodeupdown"
      fi
  fi

  AC_SUBST(HAVE_LIBGENDERS)
  AC_SUBST(GENDERS_LIBS)
  AC_SUBST(HAVE_LIBNODEUPDOWN)
  AC_SUBST(NODEUPDOWN_LIBS)

])

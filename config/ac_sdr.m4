##*****************************************************************************
## $Id$
##*****************************************************************************
#  AUTHOR:
#    Jim Garlick <garlick@llnl.gov>
#
#  SYNOPSIS:
#    AC_SDR
#
#  DESCRIPTION:
#    Adds support for SDR if found.
#
#  WARNINGS:
#    This macro must be placed after AC_PROG_CC or equivalent.
##*****************************************************************************

AC_DEFUN([AC_SDR],
[
 
  #
  # Find path to SDRGetObjects.  SDR module is automatically build if found
  #
  AC_PATH_PROG([SDRGETOBJECTS], [SDRGetObjects], [], [/usr/lpp/ssp/bin:$PATH])
  if test -n "$SDRGETOBJECTS"; then
      AC_ADD_STATIC_MODULE("sdr")
      AC_DEFINE([HAVE_SDR], [1], [Define if you have SDR])
      AC_DEFINE_UNQUOTED(_PATH_SDRGETOBJECTS, "$SDRGETOBJECTS",
                         [Path to SDRGetObjects])
  fi

  AC_SUBST(HAVE_SDR)
  AC_SUBST(SDRGETOBJECTS)

])

##*****************************************************************************
## $Id: ac_socklen_t.m4 552 2003-07-03 00:23:32Z grondo $
##*****************************************************************************
#  AUTHOR:
#   Mark Grondona <mgrondona@llnl.gov>
#
#  SYNOPSIS:
#    AC_MSGHDR_ACCRIGHTS
#
#  DESCRIPTION:
#    Check whether sys/socket.h defines msghdr with accrights field.
#    Please note that some systems require sys/types.h to be included 
#    before sys/socket.h can be compiled.
##*****************************************************************************

AC_DEFUN([AC_MSGHDR_ACCRIGHTS],
[AC_CACHE_CHECK([for msg_accrights in struct msghdr], ac_cv_msghdr_accrights,
[
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <sys/types.h>
   #include <sys/socket.h>]], [[struct msghdr m; m.msg_accrights = 0]])],[ac_cv_msghdr_accrights=yes],[ac_cv_msghdr_accrights=no])
])

if test "$ac_cv_msghdr_accrights" = "yes"; then
  AC_DEFINE([HAVE_MSGHDR_ACCRIGHTS], [1], [Define if struct msghdr has msg_accrights])
fi
])

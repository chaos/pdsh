##*****************************************************************************
## $Id$
##*****************************************************************************
#  AUTHOR:
#    Albert Chu <chu11@llnl.gov> 
#
#  SYNOPSIS:
#    AC_POLLSELECT
#
#  DESCRIPTION:
#    Checks for poll() and select() and determines which to use.
#
#  WARNINGS:
#    This macro must be placed after AC_PROG_CC or equivalent.
##*****************************************************************************

AC_DEFUN([AC_POLLSELECT],
[
   AC_CHECK_FUNC([poll], [ac_have_poll=yes], [ac_have_poll=no])
   if test "$ac_have_poll" = "yes" ; then
      AC_DEFINE([HAVE_POLL], [1], [Define that you will use poll()])
   else
      AC_CHECK_FUNC([select], [ac_have_select=yes], [ac_have_select=no])
      if test "$ac_have_select" = "yes" ; then
         AC_MSG_WARN([System does not support poll(), default to select()])
         AC_DEFINE([HAVE_SELECT], [1], [Define that you will use select()])
     else 
         AC_MSG_ERROR([System does not support select() or poll(),
                       get a real operating system!!!])
     fi
   fi
])

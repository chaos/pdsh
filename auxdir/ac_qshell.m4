##*****************************************************************************
## $Id$
##*****************************************************************************
#  AUTHOR:
#    Jim Garlick <garlick@llnl.gov>
#
#  SYNOPSIS:
#    AC_QSHELL
#
#  DESCRIPTION:
#    Adds support for the "--with-qshell" configure script option.
#    Checks for rmscall and elan3 libraries if Elan support is desired,
#    Elan-specific libraries are exported via ELAN_LIBS variable.
#
#  WARNINGS:
#    This macro must be placed after AC_PROG_CC or equivalent.
##*****************************************************************************

AC_DEFUN([AC_QSHELL],
[
  AC_MSG_CHECKING([for whether to build qshell module and qshd daemon])
  AC_ARG_WITH([qshell],
    AC_HELP_STRING([--with-qshell], [Build qsh module and qshd daemon]),
    [ case "$withval" in
        yes) ac_with_qshell=yes ;;
        no)  ac_with_qshell=no ;;
        *)   AC_MSG_RESULT([doh!])
             AC_MSG_ERROR([bad value "$withval" for --with-qshell]) ;;
      esac
    ]
  )
  AC_MSG_RESULT([${ac_with_qshell=no}])

  if test "$ac_with_qshell" = yes; then

     AC_CHECK_LIB([rmscall], [rms_prgcreate], [ac_have_rms=yes])
     AC_CHECK_LIB([elan3],   [elan3_create],  [ac_have_elan=yes])

     if test "$ac_have_rms" != "yes" ; then
        AC_MSG_NOTICE([Cannot support qshell without librmscall])
     fi

     if test "$ac_have_elan" != "yes" ; then
        AC_MSG_NOTICE([Cannot support qshell without libelan3])
     fi

     if test "$ac_with_pam" = "yes" ; then
        AC_CHECK_LIB([pam], [pam_start], [ac_have_pam=yes])
        if test "$ac_have_pam" != "yes" ; then
           AC_MSG_NOTICE([Cannot support qshell without libpam])
        fi
     else
        ac_have_pam=yes         
     fi

     if test "$ac_have_rms" = "yes" && 
        test "$ac_have_elan" = "yes" &&
        test "$ac_have_pam" = "yes" ; then
        ac_have_qshell=yes
        ac_have_qsw=yes
        AC_ADD_STATIC_MODULE("qcmd")
        QSHELL_LIBS="-lrmscall -lelan3"
        AC_DEFINE_UNQUOTED(HAVE_QSHELL, [1], [Define for Qshell support.])
        PROG_QSHD=in.qshd
        if test "$ac_with_pam" = "yes" ; then
           QSHELL_LIBS="$QSHELL_LIBS -lpam -lpam_misc"
           AC_DEFINE_UNQUOTED(USE_PAM, [1], [Define for Qshell PAM support.])
        fi
     fi
  fi

  AC_SUBST(HAVE_QSHELL)
  AC_SUBST(PROG_QSHD)
  AC_SUBST(QSHELL_LIBS)
])

##*****************************************************************************
## $Id$
##*****************************************************************************
#  AUTHOR:
#    Jim Garlick <garlick@llnl.gov>
#
#  SYNOPSIS:
#    AC_ELAN
#
#  DESCRIPTION:
#    Adds support for the "--with-elan" configure script option.
#    Checks for rmscall and elan3 libraries if Elan support is desired,
#    Elan-specific libraries are exported via ELAN_LIBS variable.
#
#  WARNINGS:
#    This macro must be placed after AC_PROG_CC or equivalent.
##*****************************************************************************

AC_DEFUN([AC_ELAN],
[
  AC_MSG_CHECKING([whether to support Elan interconnect])
  AC_ARG_WITH([elan],
    AC_HELP_STRING([--with-elan], [Include QSW Elan support]),
    [ case "$withval" in
        yes) ac_with_elan=yes ;;
        no)  ac_with_elan=no ;;
        *)   AC_MSG_RESULT([doh!])
             AC_MSG_ERROR([bad value "$withval" for --with-elan]) ;;
      esac
    ]
  )
  AC_MSG_RESULT([${ac_with_elan=no}])

  if test "$ac_with_elan" = yes; then

     AC_CHECK_LIB([rmscall], [rms_prgcreate], 
        AC_CHECK_LIB([elan3], [elan3_create], [ac_have_elan=yes]))

     if test "$ac_have_elan" = "yes" ; then
        # compile libqsw
        ac_have_qsw=yes
        AC_ADD_STATIC_MODULE("qcmd")
        ELAN_LIBS="-lrmscall -lelan3"
        AC_DEFINE_UNQUOTED(HAVE_ELAN, [1], [Define for Elan support.])
	PROG_QSHD=in.qshd
     fi
  fi
  AC_SUBST(HAVE_ELAN)
  AC_SUBST(PROG_QSHD)
  AC_SUBST(ELAN_LIBS)
])

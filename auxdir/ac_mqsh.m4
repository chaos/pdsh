##*****************************************************************************
## $Id$
##*****************************************************************************
#  AUTHOR:
#    Albert Chu <chu11@llnl.gov>
#
#  SYNOPSIS:
#    AC_MQSH
#
#  DESCRIPTION:
#    Checks for mqsh option, and sets up nettools definitions
#
#  WARNINGS:
#    This macro must be placed after AC_PROG_CC or equivalent.
##*****************************************************************************

AC_DEFUN([AC_CHECK_AF_DECL],
[
  # IEEE standard says AF_X definitions should be in sys/socket.h
  AC_CHECK_DECL($1, AC_DEFINE($2,1,[have $1]),,[#include <sys/socket.h>])
])

AC_DEFUN([AC_NETTOOLS],
[
  # The net-tools library was originally not autoconf/automake
  # configured and configuration #defines were set through manual
  # configuration.
  #
  # Luckily, the net-tools library does use "#include "config.h""
  # for its configuration.  So we can autoconf configure net-tools by 
  # having the source files use our config.h instead.
  #
  # However, we must create #defines that match the ones the library
  # specifically uses.  We can't use the standard ones created from
  # autoconf macros.
  #

  # check for domain types 
  AC_CHECK_AF_DECL([AF_UNIX],   [HAVE_AFUNIX])
  AC_CHECK_AF_DECL([AF_INET],   [HAVE_AFINET])
  AC_CHECK_AF_DECL([AF_INET6],  [HAVE_AFINET6])
  AC_CHECK_AF_DECL([AF_IPX],    [HAVE_AFIPX])
  AC_CHECK_AF_DECL([AF_ATALK],  [HAVE_AFATALK])
  AC_CHECK_AF_DECL([AF_AX25],   [HAVE_AFAX25])
  AC_CHECK_AF_DECL([AF_NETROM], [HAVE_AFNETROM])
  AC_CHECK_AF_DECL([AF_ROSE],   [HAVE_AFROSE])
  AC_CHECK_AF_DECL([AF_X25],    [HAVE_AFX25])
  AC_CHECK_AF_DECL([AF_ECONET], [HAVE_AFECONET])
  AC_CHECK_AF_DECL([AF_DECnet], [HAVE_AFDECnet])
  AC_CHECK_AF_DECL([AF_ASH],    [HAVE_AFASH])

  # define all hardware.  All HW headers should exist even if kernel
  # modules are not loaded or installed
  AC_DEFINE(HAVE_HWETHER,1,[have ethernet])
  AC_DEFINE(HAVE_HWARC,1,[have ARCnet])
  AC_DEFINE(HAVE_HWSLIP,1,[have SLIP])
  AC_DEFINE(HAVE_HWPPP,1,[have PPP])
  AC_DEFINE(HAVE_HWTUNNEL,1,[have IPIP])
  AC_DEFINE(HAVE_HWSTRIP,1,[have STRIP])
  AC_DEFINE(HAVE_HWTR,1,[have Token Ring])
  AC_DEFINE(HAVE_HWAX25,1,[have AX25])
  AC_DEFINE(HAVE_HWROSE,1,[have Rose])
  AC_DEFINE(HAVE_HWNETROM,1,[have NET/ROM])
  AC_DEFINE(HAVE_HWX25,1,[have X.25])
  AC_DEFINE(HAVE_HWFR,1,[have DLCI/FRAD])
  AC_DEFINE(HAVE_HWSIT,1,[have SOT])
  AC_DEFINE(HAVE_HWFDDI,1,[have FDDI])
  AC_DEFINE(HAVE_HWHIPPI,1,[have HIPPI])
  AC_DEFINE(HAVE_HWASH,1,[have ASH])
  AC_DEFINE(HAVE_HWHDLCLAPB,1,[have HDLC/LAPB])
  AC_DEFINE(HAVE_HWIRDA,1,[have IrDA])
  AC_DEFINE(HAVE_HWEC,1,[have Econet])
])

AC_DEFUN([AC_MQSH],
[
  #
  # Check for whether to include mqsh module
  # Assumes this is AFTER --with-elan and --with-mrsh checks
  #
  AC_MSG_CHECKING([for whether to build mqsh module])
  AC_ARG_WITH([mqsh],
    AC_HELP_STRING([--with-mqsh], [Build mqsh module and mqshd daemon]),
    [ case "$withval" in
        no)  ac_with_mqsh=no ;;
        yes) ac_with_mqsh=yes ;;
        *)   AC_MSG_RESULT([doh!])
             AC_MSG_ERROR([bad value "$withval" for --with-mqsh]) ;;
      esac
    ]
  )
  AC_MSG_RESULT([${ac_with_mqsh=no}])
   
  if test "$ac_with_mqsh" = "yes"; then
        
    # check for elan libs if --with-elan was not specified
    if test "$ac_have_elan" = "yes"; then 
       ac_mqsh_elan=yes
    else
       AC_CHECK_LIB([rmscall], [rms_prgcreate], 
              AC_CHECK_LIB([elan3], [elan3_create], [ac_mqsh_have_elan=yes]))
        
       if test "$ac_mqsh_have_elan" = "yes" ; then
          ELAN_LIBS="-lrmscall -lelan3" 
          AC_SUBST(ELAN_LIBS)
          ac_mqsh_elan=yes
       else
          ac_mqsh_elan=no
       fi
    fi

    # check for munge libs if --with-mrsh was not specified
    if test "$ac_have_libmunge" = "yes"; then 
       ac_mqsh_munge=yes
    else
       AC_CHECK_LIB([munge], [munge_encode], [ac_mqsh_have_munge=yes])

       if test "$ac_mqsh_have_munge" = "yes" ; then
          MRSH_LIBS="-lmunge" 
          AC_SUBST(MRSH_LIBS)
          ac_mqsh_munge=yes
       else
          ac_mqsh_munge=no
       fi
    fi
                         
    # check if mrsh is installed if --with-mrsh was not specified
    if test "$ac_have_mrsh" = "yes" ; then
       ac_mqsh_mrsh=yes
    else
       AC_MSG_CHECKING([for mshell in /etc/services])
       if grep "^mshell" /etc/services > /dev/null 2>&1; then
          ac_mqsh_mrsh=yes
       else
          ac_mqsh_mrsh=no
       fi     
       AC_MSG_RESULT([${ac_mqsh_mrsh=no}])
    fi

    # do we have everything we want?
    if test "$ac_mqsh_elan" = "yes" &&
       test "$ac_mqsh_munge" = "yes" &&
       test "$ac_mqsh_mrsh" = "yes" ; then
       ac_have_mqsh=yes
       AC_DEFINE([HAVE_MQSH], [1], [Define if you have mqsh.])
       PROG_MQSHD=in.mqshd   
       AC_NETTOOLS

       # compile libqsw
       ac_have_qsw=yes
    fi
  else
    ac_have_mqsh=no
  fi      

  AC_SUBST(PROG_MQSHD)
  AC_SUBST(HAVE_MQSH)
])

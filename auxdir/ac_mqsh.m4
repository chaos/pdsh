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
#    Checks for mqsh option and checks for things mqshd requires
#
#  WARNINGS:
#    This macro must be called after AC_ELAN and AC_MRSH
#    This macro must be placed after AC_PROG_CC or equivalent.
##*****************************************************************************

# Found online, original author not known
AC_DEFUN([AC_STRUCT_SA_LEN],
[
  AC_CACHE_CHECK([for sa_len in struct sockaddr], ac_cv_struct_sa_len,
        AC_TRY_COMPILE([#include <sys/types.h> #include <sys/socket.h>], 
                        [struct sockaddr s; s.sa_len;],
                        ac_cv_struct_sa_len=yes, 
                        ac_cv_struct_sa_len=no))

  if test $ac_cv_struct_sa_len = yes; then
     AC_DEFINE(HAVE_SA_LEN, [1], [do we have sa_len in struct sockaddr])  
  fi
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
       AC_ADD_STATIC_MODULE("mqcmd")
       AC_DEFINE([HAVE_MQSH], [1], [Define if you have mqsh.])
       PROG_MQSHD=in.mqshd   
       AC_STRUCT_SA_LEN

       # check for IPv6, IEEE standard says it should be in sys/socket.h 
       AC_CHECK_DECL([AF_INET6], 
                     AC_DEFINE(HAVE_IPV6,1,[have IPv6]),,
                     [#include <sys/socket.h>])  

       # compile libqsw
       ac_have_qsw=yes
         
    fi
  else
    ac_have_mqsh=no
  fi      

  AC_SUBST(PROG_MQSHD)
  AC_SUBST(HAVE_MQSH)
])

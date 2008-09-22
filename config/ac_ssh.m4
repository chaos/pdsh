##*****************************************************************************
## $Id$
##*****************************************************************************
#  AUTHOR:
#    Albert Chu
#
#  SYNOPSIS:
#    AC_SSH
#
#  DESCRIPTION:
#    Check if user wants to compile sshcmd 
#
#  WARNINGS:
#    This macro must be placed after AC_PROG_CC or equivalent.
##*****************************************************************************

AC_DEFUN([AC_SSH],
[
  #
  # Check for whether to include ssh module
  #
  AC_MSG_CHECKING([for whether to build ssh module])
  AC_ARG_WITH([ssh],
    AC_HELP_STRING([--with-ssh], [Build ssh module]),
    [ case "$withval" in
        no)  ac_with_ssh=no ;;
        yes) ac_with_ssh=yes ;;
        *)   AC_MSG_RESULT([doh!])
             AC_MSG_ERROR([bad value "$withval" for --with-ssh]) ;;
      esac
    ]
  )
  AC_MSG_RESULT([${ac_with_ssh=no}])
  
  if test "$ac_with_ssh" = "yes"; then
     ac_have_ssh=yes
     AC_ADD_STATIC_MODULE("sshcmd")
     AC_DEFINE([HAVE_SSH], [1], [Define if you have ssh.])
  fi
  AC_SUBST(HAVE_SSH)

  x_ac_ssh_connect_timeout_option="-oConnectTimeout=%d"

  AC_ARG_WITH(
    [ssh-connect-timeout-option],
    AS_HELP_STRING(--with-ssh-connect-timeout-option=OPT,
		           SSH option for connect timeout),
	[x_ac_ssh_connect_timeout_option=$withval])
	  
  if ! echo "$x_ac_ssh_connect_timeout_option" | grep -i ^none 2>/dev/null; then
    AC_DEFINE([SSH_HAS_CONNECT_TIMEOUT], [1], 
              [Define if SSH supports a connect timeout option.])
    AC_DEFINE_UNQUOTED(
			  [SSH_CONNECT_TIMEOUT_OPTION], 
              "$x_ac_ssh_connect_timeout_option",
			  [Define to SSH connect timeout option])
  fi
])

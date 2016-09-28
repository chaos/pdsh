##*****************************************************************************
#  AUTHOR:
#    Mattias Slabanja <don.fanucci@gmail.com>
#
#  SYNOPSIS:
#    AC_TORQUE
#
#  DESCRIPTION:
#    Checks for whether to include torque module
#
##*****************************************************************************

AC_DEFUN([AC_TORQUE],
[
  #
  # Check for whether to build torque module
  #

  AC_MSG_CHECKING([for whether to build torque module])

  AC_ARG_WITH([torque],
    AC_HELP_STRING([--with-torque],
      [support running pdsh under Torque allocation]),
      [ case "$withval" in
          yes) ac_with_torque=yes ;;
          no)  ac_with_torque=no ;;
          *)   AC_MSG_RESULT([doh!])
               AC_MSG_ERROR([bad value "$withval" for --with-torque]) ;;
        esac
      ]
  )
  AC_MSG_RESULT([${ac_with_torque=no}])

  if test "$ac_with_torque" = "yes"; then
    if pbs-config 1>/dev/null 2>&1; then
      TORQUE_LIBS=$(pbs-config --libs --libadd)
      TORQUE_CPPFLAGS=$(pbs-config --cflags)
      saveLIBS="$LIBS"
      LIBS="$TORQUE_LIBS $LIBS"
      AC_CHECK_LIB(torque, pbs_connect, [ac_have_libtorque=yes], [])
      LIBS="$saveLIBS"
      if test "$ac_have_libtorque" != "yes"; then
        AC_MSG_NOTICE([Cannot support torque without libtorque.])
      else
	ac_have_torque=yes
        AC_ADD_STATIC_MODULE("torque")
        AC_DEFINE([HAVE_TORQUE], [1], [Define if you have torque.])
      fi
    else
      AC_MSG_NOTICE([Cannot find pbs-config. Torque module will not be build.])
    fi
  fi

  AC_SUBST(HAVE_TORQUE)
  AC_SUBST(TORQUE_LIBS)
  AC_SUBST(TORQUE_CPPFLAGS)
])

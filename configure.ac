dnl autoconf configure.ac >configure
dnl autoheader configure.ac >config.h.in
dnl
dnl $Id$
dnl
dnl Copyright (C) 2002 Regents of the University of California
dnl See ./DISCLAIMER

dnl Prologue.
dnl
AC_INIT(conf.h)
AC_CONFIG_AUX_DIR(auxdir)
AC_CONFIG_HEADER(config.h)
AC_PREFIX_DEFAULT(/usr)


dnl Read package and version info from VERSION.
dnl
PROJECT="`perl -ne 'print,exit if s/^\s*NAME:\s*(\S*).*/\1/i' $srcdir/META`"
AC_DEFINE_UNQUOTED(PROJECT, "$PROJECT", [Define the project's name.])
AC_SUBST(PROJECT)
VERSION="`perl -ne 'print,exit if s/^\s*VERSION:\s*(\S*).*/\1/i' $srcdir/META`"
AC_DEFINE_UNQUOTED(VERSION, "$VERSION", [Define the project's version.])
AC_SUBST(VERSION)


dnl Check for debug vs. production compilation.
dnl This must be done before AC_PROG_CC defines its own defaults.
dnl
AC_MSG_CHECKING(whether debugging is enabled)
AC_ARG_ENABLE(debug,
  AC_HELP_STRING([--enable-debug], [enable debugging for development work]),
  [ case "$enableval" in
      yes) debug=yes ;;
      no)  debug=no ;;
      *)   AC_MSG_RESULT(doh!)
           AC_MSG_ERROR([bad value "$enableval" for --enable-debug]) ;;
    esac
  ]
)
if test "$debug" = yes; then
  CFLAGS="${CFLAGS--g -Wall}"
else
  CFLAGS="${CFLAGS--O3}"
  LDFLAGS="${LDFLAGS--s}"
  AC_DEFINE(NDEBUG, 1, [Define if you are building a production release.])
fi
AC_MSG_RESULT(${debug=no})

dnl Determine if we are compiling for Elan
dnl
AC_ARG_WITH(elan,
  AC_HELP_STRING([--with-elan], [enable pdsh to run parallel programs on Elan interconnect]),
  [ case "$withval" in
      yes) with_elan=yes ;;
      no)  with_elan=no ;;
      *)   AC_MSG_RESULT(doh!)
           AC_MSG_ERROR([bad value "$withval" for --with-elan]) ;;
    esac
  ]
)

dnl Determine if we are compiling for RMS 
dnl
AC_ARG_WITH(rms,
  AC_HELP_STRING([--with-rms], [enable pdsh to allocate nodes from RMS]),
  [ case "$withval" in
      yes) with_rms=yes ;;
      no)  with_rms=no ;; 
      *)   AC_MSG_RESULT(doh!)
           AC_MSG_ERROR([bad value "$withval" for --with-rms]) 
           ;;
    esac
  ]
)

dnl Determine if we are compiling for Kerberos IV
dnl
AC_ARG_WITH(krb4,
  AC_HELP_STRING([--with-krb4], [enable pdsh to use Kerberos IV kcmd]),
  [ case "$withval" in
      yes) with_krb4=yes ;;
      no)  with_krb4=no ;; 
      *)   AC_MSG_RESULT(doh!)
           AC_MSG_ERROR([bad value "$withval" for --with-krb4]) 
           ;;
    esac
  ]
)

dnl Determine if we are compiling for Genders
dnl
AC_ARG_WITH(genders,
  AC_HELP_STRING([--with-genders], [enable pdsh to use genders package]),
  [ case "$withval" in
      yes) with_genders=yes ;;
      no)  with_genders=no ;; 
      *)   AC_MSG_RESULT(doh!)
           AC_MSG_ERROR([bad value "$withval" for --with-genders]) 
           ;;
    esac
  ]
)

dnl Determine the system type.
dnl
AC_CANONICAL_HOST
AC_SUBST(host_cpu)
AC_DEFINE_UNQUOTED(HOST_CPU, "$host_cpu",
  [Define the canonical host CPU type.]
)
AC_SUBST(host_os)
AC_DEFINE_UNQUOTED(HOST_OS, "$host_os",
  [Define the canonical host OS type.]
)
AC_SUBST(host_vendor)
AC_DEFINE_UNQUOTED(HOST_VENDOR, "$host_vendor",
  [Define the canonical host vendor type.]
)


dnl Check for programs.
dnl
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_RANLIB
AC_CHECK_FILES
AC_CHECK_PROGS(PATH_SSH, /usr/bin/ssh /usr/local/bin/ssh, nada)
if test "$with-genders" = yes; then
  AC_CHECK_PROGS(path_nodeattr, nodeattr)
fi
if test "$with-rms" = yes; then
  AC_CHECK_PROGS(path_rmsquery, rmsquery)
fi

dnl Check for libraries.
dnl
AC_CHECK_LIB(socket, socket)
dnl libnsl is only needed if libsocket is required; this test prevents it
dnl   from being linked into the Linux executable when it is not needed.
dnl
if test "$ac_cv_lib_socket_socket" = yes; then
  AC_CHECK_LIB(nsl, inet_addr)
fi
dnl
AC_CHECK_LIB(pthread, pthread_create)
if test "$with_elan" = yes; then
  AC_CHECK_LIB(elan3, elan3_create)
  AC_CHECK_LIB(rmscall, rms_prgcreate)
fi
if test "$with_rms" = yes; then
  AC_CHECK_LIB(rmsapi, rms_allocateResource, "LIBS=-lrmsapi -lrms $LIBS")
fi
if test "$with_krb4" = yes; then
  AC_CHECK_LIB(krb, krb_sendauth, "LIBS=-lkrb -ldes $LIBS")
fi

dnl Check for header files.
dnl


dnl Check for typedefs.
dnl
AC_CHECK_TYPES(socklen_t, [], [], [#include <sys/socket.h>])


dnl Check for structures.
dnl


dnl Check for compiler characteristics.
dnl
AC_C_BIGENDIAN


dnl Check for library functions.
dnl
AC_CHECK_FUNCS( \
  gethostbyname_r \
  strerror_r \
)

dnl Check for system services.
dnl

dnl Check for system-specific stuff.
dnl

dnl Check for random silliness.
dnl
AC_CACHE_CHECK([for a sense of humor], ac_cv_humor, ac_cv_humor=yes)


dnl Add defs to config.h.
dnl
if test "$with_elan" = yes; then
  AC_DEFINE(WITH_ELAN, 1, [Define if building with Elan support.])
fi
if test "$with_rms" = yes; then
  AC_DEFINE(WITH_RMS, 1, [Define if building with RMS support.])
  AC_DEFINE(PATH_RMSQUERY, path_rmsquery, [Path to the rmsquery command])
fi
if test "$with_krb4" = yes; then
  AC_DEFINE(WITH_KRB4, 1, [Define if building with Kerberos IV support.])
fi
if test "$with_genders" = yes; then
  AC_DEFINE(WITH_GENDERS, 1, [Define if building with Genders support.])
fi

dnl Epilogue.
dnl
AC_OUTPUT_COMMANDS(echo "creating dependencies"; make depend >/dev/null)
AC_OUTPUT([
  Makefile
  pdsh.spec
])

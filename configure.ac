dnl autoconf configure.ac >configure
dnl autoheader configure.ac >conf.h.in
dnl
dnl $Id$
dnl
dnl Copyright (C) 2002 Regents of the University of California
dnl See ./DISCLAIMER

dnl Prologue.
dnl
AC_PREREQ(2.50)
AC_INIT(dsh.h)
AC_CONFIG_AUX_DIR(auxdir)
AC_CONFIG_HEADER(conf.h)
AC_PREFIX_DEFAULT(/usr)


dnl Read package and version info from VERSION.
dnl
PROJECT="`perl -ne 'print,exit if s/^\s*NAME:\s*(\S*).*/\1/i' $srcdir/META`"
AC_DEFINE_UNQUOTED(PROJECT, "$PROJECT", [Define the project's name.])
AC_SUBST(PROJECT)
VERSION="`perl -ne 'print,exit if s/^\s*VERSION:\s*(\S*).*/\1/i' $srcdir/META`"
AC_DEFINE_UNQUOTED(VERSION, "$VERSION", [Define the project's version.])
AC_SUBST(VERSION)

AC_AIX

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
      *)   AC_MSG_ERROR([bad value "$withval" for --with-elan]) ;;
    esac
  ]
)
if test "$with_elan" = yes; then
  ELAN_PROGS=qshd
  AC_SUBST(ELAN_PROGS) 
fi


dnl Determine if we are compiling for RMS 
dnl
AC_ARG_WITH(rms,
  AC_HELP_STRING([--with-rms], [enable pdsh to allocate nodes from RMS]),
  [ case "$withval" in
      yes) with_rms=yes ;;
      no)  with_rms=no ;; 
      *)   AC_MSG_ERROR([bad value "$withval" for --with-rms]) 
           ;;
    esac
  ]
)

dnl Determine if we are compiling for Kerberos IV
dnl
AC_ARG_WITH(krb4,
  AC_HELP_STRING([--with-krb4], [enable pdsh to use Kerberos IV kcmd]),
  [ case "$withval" in
      yes) with_krb4=yes; KRB4DIR=/usr/local/krb4 ;;
      no)  with_krb4=no ;; 
      *)   KRB4DIR=$withval; with_krb4=yes ;;
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
      *)   AC_MSG_ERROR([bad value "$withval" for --with-genders]) 
           ;;
    esac
  ]
)

dnl Determine path to MPICH machines file
dnl
AC_ARG_WITH(mpich-machines,
  AC_HELP_STRING([--with-mpich-machines], [get nodes from MPICH machines file]),
  [ case "$withval" in
      no)  with_mpich_machines=no ;; 
      yes) with_mpich_machines=yes;
	   MACHINES=/usr/local/etc/machines ;;
      *)   with_mpich_machines=yes;
	   MACHINES=$withval ;;
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
AC_PATH_PROG(SSH,ssh,[],/admin/bin:/usr/bin:/usr/local/bin:$PATH)
AC_PATH_PROG(NODEATTR,nodeattr,[],/admin/scripts:/usr/bin:$PATH)
AC_PATH_PROG(RMSQUERY,rmsquery,[],/usr/bin:$PATH)
AC_PATH_PROG(SDRGETOBJECTS,SDRGetObjects,[],/usr/lpp/ssp/bin:$PATH)
AC_PATH_PROG(RCP,rcp,[],/usr/bin:$PATH)

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
  AC_CHECK_LIB(rmsapi, rms_numNodes)
  AC_CHECK_LIB(rms, __builtin_new)
fi
if test "$with_krb4" = yes; then
  LIBS="$LIBS -lkrb4 -ldes";
fi

dnl Check for header files.
dnl
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS(fcntl.h sys/file.h sys/time.h unistd.h)
AC_CHECK_HEADERS(krb.h kparse.h)

dnl Check for typedefs.
dnl
AC_CHECK_TYPES(socklen_t, [], [], [#include <sys/socket.h>])
AC_C_CONST
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_UID_T


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
  strerror \
  pthread_sigmask \
  strstr
)

dnl Check for system services.
dnl

dnl Check for system-specific stuff.
dnl

dnl Add defs to config.h.
dnl
if test "$with_elan" = yes; then
  AC_DEFINE(HAVE_ELAN3, 1, [Define if building with Elan support.])
fi
if test "$with_rms" = yes; then
  AC_DEFINE(HAVE_RMS_PMANAGER, 1, [Define if building with RMS support.])
fi
if test "$with_krb4" = yes; then
  AC_DEFINE(KRB4, 1, [Define if building with Kerberos IV support.])
fi
if test "$with_genders" = yes; then
  AC_DEFINE(HAVE_GENDERS, 1, [Define if building with Genders support.])
fi
if test "$with_mpich_machines" = yes; then
  AC_DEFINE(HAVE_MACHINES, 1, [Define if building with MPICH machines support.])
fi

if test -n "$SSH"; then
  AC_DEFINE_UNQUOTED(_PATH_SSH, "$SSH", 
    [Define the path to the ssh program.])
fi
if test -n "$SDRGETOBJECTS"; then
  AC_DEFINE_UNQUOTED(_PATH_SDRGETOBJECTS, "$SDRGETOBJECTS", 
    [Define the path to the SDRGetObjects program.])
fi
if test -n "$NODEATTR"; then
  AC_DEFINE_UNQUOTED(_PATH_NODEATTR, "$NODEATTR", 
    [Define the path to the nodeattr program.])
fi
if test -n "$RMSQUERY"; then
  AC_DEFINE_UNQUOTED(_PATH_RMSQUERY, "$RMSQUERY", 
    [Define the path to the rmsquery program.])
fi
if test -n "$RCP"; then
  AC_DEFINE_UNQUOTED(_PATH_RCP, "$RCP", 
    [Define the path to the rcp program.])
fi
if test -n "$MACHINES"; then
  AC_DEFINE_UNQUOTED(_PATH_MACHINES, "$MACHINES", 
    [Define the path to the MPICH machines machines.])
fi
AC_DEFINE_UNQUOTED(RANGE_OP, "-", [Define the range separator string.])

dnl Epilogue.
dnl
AC_OUTPUT_COMMANDS(echo "creating dependencies"; make depend >/dev/null)
AC_OUTPUT([
  Makefile
  pdsh.spec
])

# 
# $Id$
#
# Copyright (C) 2000-2002 Regents of the University of California
# See ./DISCLAIMER
#
# This file is to be processed with autoconf to generate a configure script.

AC_INIT([pdsh])
AC_META
AC_CONFIG_AUX_DIR(auxdir)
AC_CONFIG_SRCDIR([dsh.h])
AC_CANONICAL_SYSTEM
AC_GPL_LICENSED

# hack to fix dejagnu.am brokenness before automake 1.6
if test x$host_alias = x ; then
   host_alias=$host_cpu
fi

#
#  Automake support
# 
AM_INIT_AUTOMAKE([$PACKAGE], [$VERSION])
AM_CONFIG_HEADER(config.h)

#
# Checks for programs.
#
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_DEBUG

#
# Libtool and ltld.[ch] support
#
AC_LIBTOOL_DLOPEN
AC_PROG_LIBTOOL
AC_LIB_LTDL

#
# Test for support of QSW Elan. If requested and available, build
#  qshd and qcmd connect module.
#
AC_ELAN
AM_CONDITIONAL(WITH_ELAN, [test $ac_with_elan = yes])

#
# Test for whether to build "machines" module.
#
AC_MACHINES
AM_CONDITIONAL(WITH_MACHINES, [test $ac_with_machines = yes])

# NOTE: support for SDR is automatic if SDRGetObjects is found
# NOTE: support for Genders is automatic if nodeattr is found

#
# Test for default pdsh fanout and connect timeout
#
AC_FANOUT
AC_CONNECT_TIMEOUT

#
# Check for genders and libnodeupdown support
#
AC_GENDERS
AM_CONDITIONAL(WITH_LIBGENDERS, [test "$ac_have_libgenders"    = "yes"])
AM_CONDITIONAL(WITH_NODEUPDOWN, [test "$ac_have_libnodeupdown" = "yes"])
AM_CONDITIONAL(WITH_NODEATTR,   [test "$ac_have_nodeattr"      = "yes"])


dnl
dnl check for whether to include readline support
dnl
AC_READLINE
AM_CONDITIONAL([WITH_READLINE], [test "$ac_with_readline" = "yes"])

#
# Find path to rmsquery.  Support for RMS (allocate) is automatic if found.
#
AC_PATH_PROG(RMSQUERY, rmsquery, [], [/usr/bin:$PATH])
if test -n "$RMSQUERY"; then
    AC_DEFINE_UNQUOTED(_PATH_RMSQUERY, "$RMSQUERY", [Path to rmsquery])
fi
AM_CONDITIONAL(WITH_RMS, test -n "$HAVE_RMSQUERY")

#
# Find path to SDRGetObjects.  SDR module is automatically build if found
#
AC_PATH_PROG([SDRGETOBJECTS], [SDRGetObjects], [], [/usr/lpp/ssp/bin:$PATH])
if test -n "$SDRGETOBJECTS"; then
    AC_DEFINE_UNQUOTED(_PATH_SDRGETOBJECTS, "$SDRGETOBJECTS", 
			          [Path to SDRGetObjects])
fi
AM_CONDITIONAL(WITH_SDR, [test -n "$SDRGETOBJECTS"])

#
# Find path to rcp for pdcp.
#
AC_PATH_PROG([RCP], [rcp], [], [/usr/bin:$PATH])
if test -n "$RCP"; then
    AC_DEFINE_UNQUOTED(_PATH_RCP, "$RCP", [Path to rcp])
fi

# Checks for libraries.
AC_CHECK_LIB([socket], [socket], LIBS="-lsocket -lnsl $LIBS",, [-lsocket -lnsl])


# Check for how to compile pthread programs:
ACX_PTHREAD
AC_DEFINE(WITH_PTHREADS, 1, [Define if you have pthreads])

# PTHREAD_CFLAGS needs to be appended to both LDFLAGS and CPPFLAGS or some
# checks for headers may fail later on (e.g. on OSF systems where -pthread
# is required in order to include pthread.h)
LDFLAGS="$LDFLAGS $PTHREAD_CFLAGS"
CPPFLAGS="$CPPFLAGS $PTHREAD_CFLAGS"
LIBS="$PTHREAD_LIBS $LIBS"


#
# Check for kerberos libraries
#
AC_CHECK_LIB([krb], [krb_sendauth], [KRB_LIBS="-lkrb -ldes"; withkrb4=yes],
	[withkrb4=no], [-lkrb -ldes])
AC_SUBST(KRB_LIBS)
AM_CONDITIONAL(WITH_KRB4, test $withkrb4 = yes)



# Checks for header files.
AC_CHECK_HEADERS([fcntl.h strings.h sys/file.h unistd.h pthread.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_TYPES([socklen_t])

# Checks for library functions.
dnl AC_FUNC_MALLOC
AC_FUNC_STRERROR_R
AC_CHECK_FUNCS([strerror pthread_sigmask sigthreadmask])

# Check for Gray Watson's Debug Malloc Library <http://dmalloc.com/>.
AC_MSG_CHECKING(whether to use the Debug Malloc Library)
AC_ARG_WITH(dmalloc,
  AC_HELP_STRING([--with-dmalloc], [use Gray Watson's dmalloc library]),
  [ case "$withval" in
      yes) dmalloc=yes ;;
      no)  dmalloc=no ;;
      *)   AC_MSG_RESULT(doh!)
           AC_MSG_ERROR([bad value "$withval" for --with-dmalloc]) ;;
    esac
  ]
)
AC_MSG_RESULT(${dmalloc=no})
if test "$dmalloc" = yes; then
  AC_CHECK_LIB([dmalloc],[main],[ AC_DEFINE_UNQUOTED(WITH_DMALLOC, 1,
        [Define if using the debug malloc library.]
      )
      LIBS="-ldmalloc $LIBS"
    ],[dmalloc=no
  ],[])
ac_cv_lib_dmalloc=ac_cv_lib_dmalloc_main

fi
AH_BOTTOM(
[#ifdef WITH_DMALLOC
#  include <stdlib.h>
#  include <dmalloc.h>
#endif /* WITH_DMALLOC */]
)

# Should probably be defining tests for these - cheat for now
AH_BOTTOM(
[#ifdef _AIX
#  define HAVE_MTSAFE_GETHOSTBYNAME 1
#  define HAVE_MAGIC_RSHELL_CLEANUP 1
#  define WANT_RECKLESS_HOSTRANGE_EXPANSION 1
#else
#  define HAVE_MTSAFE_GETHOSTBYNAME 0
#  define HAVE_MAGIC_RSHELL_CLEANUP 0
#  define WANT_RECKLESS_HOSTRANGE_EXPANSION 0
#endif /* _AIX */]
)

AC_CONFIG_FILES([ 
  Makefile 
  testsuite/Makefile 
  modules/Makefile
  testsuite/pdsh.test/Makefile 
  pdsh.spec 
  pdcp.1 pdsh.1
 ]
)
AC_OUTPUT

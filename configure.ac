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



#
# Test for default pdsh fanout and connect timeout
#
AC_FANOUT
AC_CONNECT_TIMEOUT

# NOTE: support for SDR is automatic if SDRGetObjects is found

#
# Test for support of QSW Elan. If requested and available, build
#  qshd and qcmd connect module.
#
# achu - WITH_QSW builds libqsw.  It is a separate conditional, b/c
#        libqsw may be needed for mqsh (--with-mqsh) even if 
#        --with-elan is not specified
#
AC_ELAN
AM_CONDITIONAL(WITH_ELAN, [test "$ac_have_elan" = "yes"])
AM_CONDITIONAL(WITH_QSW,  [test "$ac_have_qsw"  = "yes"])

#
# Test for whether to build "machines" module.
#
AC_MACHINES
AM_CONDITIONAL(WITH_MACHINES, [test "$ac_with_machines" = "yes"])

#
# Check for genders and libnodeupdown support
#
AC_GENDERS
AM_CONDITIONAL(WITH_LIBGENDERS, [test "$ac_have_libgenders"    = "yes"])
AM_CONDITIONAL(WITH_NODEUPDOWN, [test "$ac_have_libnodeupdown" = "yes"])
AM_CONDITIONAL(WITH_NODEATTR,   [test "$ac_have_nodeattr"      = "yes"])

#
# Check for libmunge and mrsh
#
AC_MRSH
AM_CONDITIONAL(WITH_LIBMUNGE,   [test "$ac_have_libmunge"      = "yes"])
AM_CONDITIONAL(WITH_MRSH,       [test "$ac_have_mrsh"          = "yes"])

#
# Check if user wants to build mqsh
# Run only after checks for --with-elan and --with-mrsh 
#
AC_MQSH
AM_CONDITIONAL(WITH_MQSH,       [test "$ac_have_mqsh"          = "yes"])
AM_CONDITIONAL(WITH_QSW,        [test "$ac_have_qsw"           = "yes"])

#
# Determine whether to build RMS module (Support for RMS allocate)
#
AC_RMSQUERY
AM_CONDITIONAL([WITH_RMS], [test "$ac_have_rmsquery" = "yes"])

dnl
dnl check for whether to include readline support
dnl
AC_READLINE
AM_CONDITIONAL([WITH_READLINE], [test "$ac_with_readline" = "yes"])

dnl
dnl check for inclusion of Dmalloc. 
dnl Note: this macro defines WITH_DMALLOC for us.
dnl 
AC_DMALLOC

#
# Find path to SDRGetObjects.  SDR module is automatically build if found
#
AC_PATH_PROG([SDRGETOBJECTS], [SDRGetObjects], [], [/usr/lpp/ssp/bin:$PATH])
if test -n "$SDRGETOBJECTS"; then
    AC_DEFINE([HAVE_SDR], [1], [Define if you have SDR])
    AC_DEFINE_UNQUOTED(_PATH_SDRGETOBJECTS, "$SDRGETOBJECTS", 
			          [Path to SDRGetObjects])
fi
AC_SUBST(HAVE_SDR)
AC_SUBST(SDRGETOBJECTS)
AM_CONDITIONAL(WITH_SDR, [test -n "$SDRGETOBJECTS"])

#
# Find path to rcp for pdcp.
#
AC_PATH_PROG([RCP], [rcp], [], [/usr/bin:$PATH])
if test -n "$RCP"; then
    AC_DEFINE_UNQUOTED(_PATH_RCP, "$RCP", [Path to rcp])
fi


#
# Build PDSH_VERSION string
#
if test $RELEASE = 1; then
   PDSH_VERSION="$PACKAGE-$VERSION"
else
   PDSH_VERSION="$PACKAGE-$VERSION-$RELEASE"
fi

if test "$ac_debug" = "yes"; then
   PDSH_VERSION="$PDSH_VERSION +debug"
fi

if test "$ac_with_dmalloc" = "yes"; then
   PDSH_VERSION="$PDSH_VERSION +dmalloc"
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
  net-toollib/Makefile
  net-toollib/include/Makefile
  net-toollib/lib/Makefile
  pdsh.spec 
  pdcp.1 pdsh.1
 ]
)
AC_OUTPUT

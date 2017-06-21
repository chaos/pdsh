# 
# $Id$
#
# Copyright (C) 2000-2002 Regents of the University of California
# See ./DISCLAIMER
#
# This file is to be processed with autoconf to generate a configure script.

AC_INIT([pdsh],
        m4_esyscmd([git describe --always |  awk '/.*/ {sub(/^v/, ""); printf "%s",$1; exit}']))

AC_CONFIG_SRCDIR([pdsh])
X_AC_META
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_SRCDIR([src/pdsh/dsh.h])
AC_CANONICAL_TARGET
AC_GPL_LICENSED

# hack to fix dejagnu.am brokenness before automake 1.6
if test x$host_alias = x ; then
   host_alias=$host_cpu
fi

#
#  Automake support
# 
AM_INIT_AUTOMAKE([no-define])
AC_CONFIG_HEADERS([config.h])
AM_MAINTAINER_MODE

#
# Checks for programs.
#
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_DEBUG
LT_INIT

AM_CONDITIONAL(WITH_GNU_LD, test "$with_gnu_ld" = "yes")


#
# Do we want static modules?
#
AC_ARG_ENABLE([static-modules],
              AS_HELP_STRING([--enable-static-modules],[Build static modules]),
              ac_static_modules=yes)
AM_CONDITIONAL(WITH_STATIC_MODULES, test "$ac_static_modules" = "yes")

if test "$ac_static_modules" = "yes" ; then
   AC_DEFINE_UNQUOTED(STATIC_MODULES, [1], [Use Static Modules])
   AC_STATIC_MODULES_INIT
else
   case "$host" in
   *-*-aix*)
     LDFLAGS="$LDFLAGS -Wl,-brtl -Wl,-bexpall" 
	 AIX_PDSH_LDFLAGS="-Wl,-bgcbypass:1000" # hack to ensure no gc in libcommon
	 AC_DEFINE(_AIX, 1, [Define if on AIX])
   ;;
   *-*-solaris*)
   	 AC_DEFINE(_POSIX_PTHREAD_SEMANTICS, 1, [POSIX pthread semantics on Solaris])
   ;;
   *-*-hpux*)
     LDFLAGS="$LDFLAGS -Wl,-E"
   ;;
   esac
fi
AC_SUBST(AIX_PDSH_LDFLAGS)


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

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h strings.h sys/file.h unistd.h features.h \
                  pthread.h poll.h sys/poll.h sys/sysmacros.h, sys/uio.h])

# Checks for typedefs, structures, and compiler characteristics.
TYPE_SOCKLEN_T
AC_SYS_LARGEFILE
AC_MSGHDR_ACCRIGHTS

# Checks for library functions.
dnl AC_FUNC_MALLOC
AC_FUNC_STRERROR_R
AC_CHECK_FUNCS([strerror pthread_sigmask sigthreadmask rresvport rresvport_af atoi])

#
# Check for poll vs. select()
#
AC_POLLSELECT

#
# Test for default pdsh fanout and connect timeout
#
AC_FANOUT
AC_CONNECT_TIMEOUT

#
# Test for inclusion of standard "rsh" module
# 
AC_RSH
AM_CONDITIONAL(WITH_RSH, [test "$ac_with_rsh" = "yes"]) 

#
# Test for inclusion of standard "xcpu" module
# 
AC_XCPU
AM_CONDITIONAL(WITH_XCPU, [test "$ac_with_xcpu" = "yes"]) 

#
# Test for ssh
#
AC_SSH
AM_CONDITIONAL(WITH_SSH, test  "$ac_have_ssh"   =  "yes")

#
# Test for exec
#
AC_EXEC
AM_CONDITIONAL(WITH_EXEC, test  "$ac_have_exec"   =  "yes")


#
# Test for kerberos
#
AC_KRB4
AM_CONDITIONAL(WITH_KRB4, test  "$ac_have_krb4" =  "yes")

#
# Test for whether to build "machines" module.
#
AC_MACHINES
AM_CONDITIONAL(WITH_MACHINES, [test "$ac_have_machines" = "yes"])

#
# Check for genders
#
AC_GENDERS
AM_CONDITIONAL(WITH_LIBGENDERS, [test "$ac_have_libgenders"    = "yes"])

#
# Check for nodeupdown
#
AC_NODEUPDOWN
AM_CONDITIONAL(WITH_NODEUPDOWN, [test "$ac_have_libnodeupdown" = "yes"])

#
# Check for libmunge and mrsh
#
AC_MRSH
AM_CONDITIONAL(WITH_LIBMUNGE,   [test "$ac_have_libmunge"      = "yes"])
AM_CONDITIONAL(WITH_MRSH,       [test "$ac_have_mrsh"          = "yes"])

#
# Determine whether to build SLURM module (support running under SLURM jobid
#
AC_SLURM
AM_CONDITIONAL([WITH_SLURM], [test "$ac_have_slurm" = "yes"])

#
# Determine whether to to build Torque mudule (support running under Torque jobid)
#
AC_TORQUE
AM_CONDITIONAL([WITH_TORQUE], [test "$ac_have_torque" = "yes"])

#
# Determine whether to build dshgroup module 
# (support dsh-style /etc/dsh/group/%s ~/.dsh/group/%s files)
#
AC_DSHGROUP
AM_CONDITIONAL([WITH_DSHGROUP], [test "$ac_with_dshgroup" = "yes"])

AC_NETGROUP
AM_CONDITIONAL([WITH_NETGROUP], [test "$ac_with_netgroup" = "yes"])

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
#  Check for alternate rcmd rank list:
#
AC_RCMD_RANK_LIST

if test "$ac_static_modules" = "yes" ; then
   AC_STATIC_MODULES_EXIT
fi

#
# Build PDSH_VERSION string
#
if echo "$META_RELEASE" | grep '^[[0-9]][[0-9]]*$' >/dev/null 2>&1; then
   PDSH_VERSION="$META_NAME-$META_VERSION"
else
   if echo "$META_RELEASE" | grep -e "pre0" -e "UNSTABLE" >/dev/null 2>&1; then
      if test "$META_RELEASE" = "UNSTABLE"; then
	     DATE=`date +"%Y%m%d%H%M"`
      else
	     DATE=`echo $META_RELEASE | cut -d. -f3`
      fi
      PDSH_VERSION="$META_NAME-$META_VERSION (unstable cvs build $DATE)"
   else
      PDSH_RELEASE="`echo $META_RELEASE | sed 's/^[[^\.]]*\.//'`"
      PDSH_VERSION="$META_NAME-$META_VERSION-$PDSH_RELEASE"
   fi
fi
test "$ac_static_modules" = "yes" && EXTRA_VERS="+static-modules"
test "$ac_with_readline"  = "yes" && EXTRA_VERS="${EXTRA_VERS}+readline"
test "$ac_debug"          = "yes" && EXTRA_VERS="${EXTRA_VERS}+debug"
test "$ac_with_dmalloc"   = "yes" && EXTRA_VERS="${EXTRA_VERS}+dmalloc"

if test -n "$EXTRA_VERS" ; then
   PDSH_VERSION_FULL="$PDSH_VERSION ($EXTRA_VERS)"
else
   PDSH_VERSION_FULL="$PDSH_VERSION"
fi

AC_SUBST(PDSH_VERSION)
AC_SUBST(PDSH_VERSION_FULL)


AC_DEFINE(WITH_LSD_FATAL_ERROR_FUNC, 1, [Have definition of lsd_fatal_error])
AC_DEFINE(WITH_LSD_NOMEM_FUNC, 1, [Have definition of lsd_nomem_error])


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
  config/Makefile
  src/Makefile
  src/common/Makefile
  src/pdsh/Makefile
  src/modules/Makefile
  doc/Makefile
  scripts/Makefile
  tests/Makefile
  tests/test-modules/Makefile
  doc/pdcp.1 
  doc/pdsh.1
 ]
)
AC_OUTPUT

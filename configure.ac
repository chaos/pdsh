# 
# $Id$
#
# Copyright (C) 2000-2002 Regents of the University of California
# See ./DISCLAIMER
#
# Process this file with autoconf to produce a configure script.
# NOTE: We distribute ./configure and do not expect people building pdsh
# to have to regenerate from configure.ac.  However, if this is necessary,
# the ./mkconfig script is provided.  You may need autoconf 2.52 or greater
# (It does blow up with 2.13 on AIX).

AC_INIT
AC_CONFIG_SRCDIR([dsh.h])
AC_CONFIG_AUX_DIR(auxdir)
AC_SUBST(ac_aux_dir)
AC_CONFIG_SRCDIR([configure.ac])

# determine the system type
AC_CANONICAL_TARGET
AC_SUBST(host_cpu)

# hack to fix dejagnu.am brokenness before automake 1.6
if test x$host_alias = x ; then
   host_alias=$host_cpu
fi

#
# Determine project/version from META file.
# These are substituted into the Makefile and config.h.
#
PROJECT="`perl -ne 'print,exit if s/^\s*NAME:\s*(\S*).*/\1/i' $srcdir/META`"
AC_DEFINE_UNQUOTED(PROJECT, "$PROJECT", [Define the project's name.])
AC_SUBST(PROJECT)

# Automake desires "PACKAGE" variable instead of PROJECT
PACKAGE=$PROJECT

VERSION="`perl -ne 'print,exit if s/^\s*VERSION:\s*(\S*).*/\1/i' $srcdir/META`"
AC_DEFINE_UNQUOTED(VERSION, "$VERSION", [Define the project's version.])
AC_SUBST(VERSION)

RELEASE="`perl -ne 'print,exit if s/^\s*RELEASE:\s*(\S*).*/\1/i' $srcdir/META`"
AC_DEFINE_UNQUOTED(RELEASE, "$RELEASE", [Define the project's release.])
AC_SUBST(RELEASE)

dnl Initialize Automake
dnl
AM_INIT_AUTOMAKE($PROJECT, $VERSION)
AM_CONFIG_HEADER(config.h)

#
# Configure arg: --enable-debug
#
# Check for debug vs. production compilation.
# This must be done before AC_PROG_CC defines its own defaults.
#
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

# 
# Configure arg: --with-elan
#
# Build qshd/qcmd and link against elan libs.  This enables the pdsh -E option,
# in addition to the connect method used when -E is not there 
# (e.g. ssh or rsh).
# XXX pam support is not yet working in qshd.
#
AC_MSG_CHECKING(whether to support Elan interconnect)
AC_ARG_WITH(elan,
    AC_HELP_STRING([--with-elan], [Include QSW Elan support]),
    [ case "$withval" in
	yes)
	    withelan=yes
	    HAVE_ELAN=1
	    AC_DEFINE_UNQUOTED(HAVE_ELAN, 1, [define for Elan support])
#	    AC_DEFINE_UNQUOTED(USE_PAM, 1, [define for PAM support in qshd])
	    PROG_QSHD=in.qshd
	    ;;
	no)
	    ;;
	*)
	    AC_MSG_RESULT(doh!)
	    AC_MSG_ERROR([bad value "$withval" for --with-elan])
	    ;;
    esac ]
)
AC_MSG_RESULT(${withelan=no})
AC_SUBST(PROG_QSHD)
: ${HAVE_ELAN=0}
AC_SUBST(HAVE_ELAN)
AM_CONDITIONAL(WITH_ELAN, test $withelan = yes)

#
# Configure arg: --with-ssh=[/path/to/ssh]  
#
# Use an external ssh program to connect.  Ssh then replaces rsh as the
# default remote shell mechanism.  
# XXX Note that arguments are subtly different between ssh and ssh2 versions, 
# and sshcmd.c checks the executable name to select between the two option sets.
# NOTE: --with-krb4 and --with-ssh are mutually exclusive (caught below)
#
AC_MSG_CHECKING(whether to use SSH as primary remote shell)
AC_ARG_WITH(ssh,
    AC_HELP_STRING([--with-ssh], [Use SSH as primary remote shell]),
    [ case "$withval" in
	yes)
	    withssh=yes
	    ;;
	no)
	    ;;
	*)
	    withssh=yes
	    SSH=$withval
	    ;;
    esac ]
)
AC_MSG_RESULT(${withssh=no})
if test "$withssh" = yes; then
    AC_DEFINE(HAVE_SSH, 1, [Define to use SSH as primary remote shell])
    HAVE_SSH=1
    # Note: if ssh is execute-only this may fail (See gnats: pdsh/81)
    if test -n "$SSH"; then
        AC_CHECK_FILE($SSH, [], AC_MSG_ERROR([--with-ssh argument not found]))
    fi
fi
: ${HAVE_SSH=0}
AC_SUBST(HAVE_SSH)
AM_CONDITIONAL(WITH_SSH, test $withssh = yes)

#
# Configure arg: --with-krb4=[/krb4/root/dir]
#
# Use kerberos IV remote shell protocol to connect.  If configured, this 
# replaces rsh as the default remote shell mechanism.  This has only ever
# been used on IBM SP's and not in a while at LLNL (k4 KDC did not handle
# many simultaneous queries SP).
# NOTE: --with-krb4 and --with-ssh are mutually exclusive (caught below)
# 
AC_MSG_CHECKING(whether to include Kerberos IV support)
AC_ARG_WITH(krb4,
    AC_HELP_STRING([--with-krb4], [Include Kerberos IV support]),
    [ case "$withval" in
	no)
	    ;;
	yes)
	    withkrb4=yes
	    HAVE_KRB4=1
	    ;;
	*)
	    withkrb4=yes
	    LIBS="$LIBS -L${withval}/lib"
	    CPPFLAGS="$CPPFLAGS -I${withval}/include"
	    ;;
    esac ]
)
AC_MSG_RESULT(${withkrb4=no})
if test "$withkrb4" = yes; then
    AC_DEFINE(HAVE_KRB4, 1, [define for Kerberos IV support])
fi
: ${HAVE_KRB4=0}
AC_SUBST(HAVE_KRB4)
AM_CONDITIONAL(WITH_KRB4, test $withkrb4 = yes)

# Detect attempt to configure --with-krb4 and --with-ssh (pick one!)
if test "$withkrb4" = yes && test "$withssh" = yes; then
    AC_MSG_ERROR([--with-krb4 and --with-ssh cannot both be specified])
fi

# NOTE: support for SDR is automatic if SDRGetObjects is found
# NOTE: support for Genders is automatic if nodeattr is found

# --with-machines=/path/to/machines
# This overrides nodeattr and SDRGetObjects if configured.
AC_MSG_CHECKING(whether to use machines file)
AC_ARG_WITH(machines,
    AC_HELP_STRING([--with-machines], [Specify a flat file list of all nodes]),
    [ case "$withval" in
	no)
	    ;;
	yes)
	    AC_MSG_RESULT(doh!)
	    AC_MSG_ERROR([--with-machines requires an argument])
	    ;;
	*)
	    withmachines=yes
	    HAVE_MACHINES=1
	    MACHINES=$withval
	    AC_DEFINE(HAVE_MACHINES, 1, [Using machines file])
            AC_DEFINE_UNQUOTED(_PATH_MACHINES, "$MACHINES", 
		[Define path to flat list of nodes for -a processing])
	    ;;
    esac ]
)
AC_MSG_RESULT(${withmachines=no})
: ${HAVE_MACHINES=0}
AC_SUBST(HAVE_MACHINES)
AC_SUBST(MACHINES)

# --with-fanout=N
AC_MSG_CHECKING(for default fanout)
AC_ARG_WITH(fanout,
    AC_HELP_STRING([--with-fanout=N], [Specify default fanout]),
    [ case "$withval" in
	no)
	    FANOUT=1
	    ;;
	yes)
	    ;;
	*)
	    FANOUT=$withval
	    ;;
    esac ]
)
AC_MSG_RESULT(${FANOUT=32})
AC_DEFINE_UNQUOTED(DFLT_FANOUT, $FANOUT, [Default fanout])
AC_SUBST(FANOUT)

# --with-connect-timeout=N
AC_MSG_CHECKING(for connect timeout)
AC_ARG_WITH(connect-timeout,
    AC_HELP_STRING([--with-connect-timeout=N], 
	[Set default connect timeout (secs)]),
    [ case "$withval" in
	no)
	    CONNECT_TIMEOUT=0
	    ;;
	yes)
	    ;;
	*)
	    CONNECT_TIMEOUT=$withval
	    ;;
    esac ]
)
AC_MSG_RESULT(${CONNECT_TIMEOUT=10})
AC_DEFINE_UNQUOTED(CONNECT_TIMEOUT, $CONNECT_TIMEOUT, [Default connect timeout])
AC_SUBST(CONNECT_TIMEOUT)

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET

#
# If specified on the command line, use ssh.  Otherwise use rsh.
#
if test "$withssh" = yes; then
    if test -z "$SSH"; then
        AC_PATH_PROG(SSH, ssh, [], /admin/bin:/usr/bin:/usr/local/bin:$PATH)
    fi
    if test -n "$SSH"; then
        AC_DEFINE_UNQUOTED(_PATH_SSH, "$SSH", [Path to ssh])
    else
        AC_MSG_ERROR([--with-ssh specified but ssh program is not found])
    fi
fi
AC_SUBST(SSH)

#
# Find path to nodeattr.  Support for genders (-g|-a) is automatic if found.
#
AC_PATH_PROG(NODEATTR, nodeattr, [], /usr/bin:/admin/scripts:$PATH)
if test -n "$NODEATTR"; then
    AC_DEFINE_UNQUOTED(_PATH_NODEATTR, "$NODEATTR", [Path to nodeattr])
    AC_DEFINE(HAVE_GENDERS, 1, [Define if Genders is available])
    HAVE_GENDERS=1
fi
: ${HAVE_GENDERS=0}
AC_SUBST(HAVE_GENDERS)
AC_SUBST(NODEATTR)
AM_CONDITIONAL(WITH_GENDERS, test $HAVE_GENDERS = 1)

#
# Find path to rmsquery.  Support for RMS (allocate) is automatic if found.
#
AC_PATH_PROG(RMSQUERY, rmsquery, [], /usr/bin:$PATH)
if test -n "$RMSQUERY"; then
    AC_DEFINE_UNQUOTED(_PATH_RMSQUERY, "$RMSQUERY", [Path to rmsquery])
    AC_DEFINE(HAVE_RMSQUERY, 1, [Define if RMS is available]) 
    HAVE_RMSQUERY=1
fi
AC_SUBST(RMSQUERY)
: ${HAVE_RMSQUERY=0}
AC_SUBST(HAVE_RMSQUERY)
AC_SUBST(RMSQUERY)
AM_CONDITIONAL(WITH_RMS, test $HAVE_RMSQUERY = 1)

#
# Find path to SDRGetObjects.  Support for SDR (-av) is automatic if found.
#
AC_PATH_PROG(SDRGETOBJECTS, SDRGetObjects, "", /usr/lpp/ssp/bin:$PATH)
if test -n "$SDRGETOBJECTS"; then
    AC_DEFINE_UNQUOTED(_PATH_SDRGETOBJECTS, "$SDRGETOBJECTS", [Path SDRGetObjects])
    AC_DEFINE(HAVE_SDR, 1, [Define if SDR is available]) 
    HAVE_SDR=1
fi
: ${HAVE_SDR=0}
AC_SUBST(HAVE_SDR)
AC_SUBST(SDRGETOBJECTS)
AM_CONDITIONAL(WITH_SDR, test $HAVE_SDR = 1)

#
# Find path to rcp for pdcp.
#
AC_PATH_PROG(RCP, rcp, [], /usr/bin:$PATH)
if test -n "$RCP"; then
    AC_DEFINE_UNQUOTED(_PATH_RCP, "$RCP", [Path to rcp])
fi
AC_SUBST(RSH)

# Checks for libraries.
AC_CHECK_LIB([socket], [socket], LIBS="-lsocket -lnsl $LIBS",, [-lsocket -lnsl])


# Check for how to compile pthread programs:
ACX_PTHREAD

# PTHREAD_CFLAGS needs to be appended to both LDFLAGS and CPPFLAGS or some
# checks for headers may fail later on (e.g. on OSF systems where -pthread
# is required in order to include pthread.h)
LDFLAGS="$LDFLAGS $PTHREAD_CFLAGS"
CPPFLAGS="$CPPFLAGS $PTHREAD_CFLAGS"
LIBS="$PTHREAD_LIBS $LIBS"
dnl CC="$PTHREAD_CC"


if test "$withelan" = yes; then
    AC_CHECK_LIB([rmscall], [rms_prgcreate],, AC_MSG_ERROR([No librmscall!]))
    AC_CHECK_LIB([elan3], [elan3_create],,AC_MSG_ERROR([No libelan3!]))
#    AC_CHECK_LIB([pam], [pam_start], LIBSPAM="-lpam -lpam_misc",
#	AC_MSG_ERROR([No libpam!]), [-lpam -lpam_misc])
fi
AC_SUBST(LIBSPAM)
if test "$withkrb4" = yes; then
    AC_CHECK_LIB([krb], [krb_sendauth], LIBS="-lkrb -ldes $LIBS",
	AC_MSG_ERROR([No libkrb!]), [-lkrb -ldes])
fi

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h strings.h sys/file.h unistd.h pthread.h])


# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_TYPE(socklen_t, int)

# Checks for library functions.
AC_FUNC_MALLOC
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

AC_CONFIG_FILES([ Makefile 
                  testsuite/Makefile 
		  testsuite/pdsh.test/Makefile 
		  pdsh.spec 
		  pdcp.1 pdsh.1
		]
)
AC_OUTPUT

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

AC_INIT(dsh.h)
AC_CONFIG_HEADER([config.h])
AC_CONFIG_AUX_DIR([auxdir])

# determine project/version from META file
PROJECT="`perl -ne 'print,exit if s/^\s*NAME:\s*(\S*).*/\1/i' $srcdir/META`"
AC_DEFINE_UNQUOTED(PROJECT, "$PROJECT", [Define the project's name.])
AC_SUBST(PROJECT)

VERSION="`perl -ne 'print,exit if s/^\s*VERSION:\s*(\S*).*/\1/i' $srcdir/META`"
AC_DEFINE_UNQUOTED(VERSION, "$VERSION", [Define the project's version.])
AC_SUBST(VERSION)

RELEASE="`perl -ne 'print,exit if s/^\s*RELEASE:\s*(\S*).*/\1/i' $srcdir/META`"
AC_DEFINE_UNQUOTED(RELEASE, "$RELEASE", [Define the project's release.])
AC_SUBST(RELEASE)

# determine the system type
AC_CANONICAL_HOST
AC_SUBST(host_cpu)

# Check for debug vs. production compilation.
# This must be done before AC_PROG_CC defines its own defaults.
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

# Enable features that interact with external software
# --with-elan
AC_MSG_CHECKING(whether to support Elan interconnect)
AC_ARG_WITH(elan,
    AC_HELP_STRING([--with-elan], [Include QSW Elan support]),
    [ case "$withval" in
	yes)
	    withelan=yes
	    HAVE_ELAN=1
	    AC_DEFINE_UNQUOTED(HAVE_ELAN, 1, [define for Elan support])
#	    AC_DEFINE_UNQUOTED(USE_PAM, 1, [define for PAM support in qshd])
	    PROG_QSHD=qshd
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

# --with-rms
AC_MSG_CHECKING(whether to support RMS)
AC_ARG_WITH(rms,
    AC_HELP_STRING([--with-rms], [Include RMS support]),
    [ case "$withval" in
	yes)
	    withrms=yes
	    HAVE_RMS=1
	    AC_DEFINE(HAVE_RMS, 1, [define for RMS support]) ;;
	no)
	    ;;
	*)
	    AC_MSG_RESULT(doh!)
	    AC_MSG_ERROR([bad value "$withval" for --with-rms])
	    ;;
    esac ]
)
AC_MSG_RESULT(${withrms=no})
: ${HAVE_RMS=0}
AC_SUBST(HAVE_RMS)

# --with-ssh
# or you can say --with-ssh=/ssh/executable
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
    if test -n "$SSH"; then
        AC_CHECK_FILE($SSH, [], AC_MSG_ERROR([--with-ssh argument not found]))
    fi
fi
: ${HAVE_SSH=0}
AC_SUBST(HAVE_SSH)

# --with-krb4
# or you can say --with-krb4=/usr/local/krb4
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

# Detect this conflict
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

# --with-rangeop=c
AC_MSG_CHECKING(for range operator)
AC_ARG_WITH(rangeop,
    AC_HELP_STRING([--with-rangeop=c], [Set range operator character]),
    [ case "$withval" in
	no)
	    RANGE_OP=""
	    ;;
	yes)
	    ;;
	*)
	    RANGE_OP="$withval"
	    ;;
    esac ]
)
AC_MSG_RESULT(${RANGE_OP=-})
AC_DEFINE_UNQUOTED(RANGE_OP, "$RANGE_OP", [Range operator])
AC_SUBST(RANGE_OP)

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET

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

AC_PATH_PROG(NODEATTR, nodeattr, [], /usr/bin:/admin/scripts:$PATH)
if test -n "$NODEATTR"; then
    AC_DEFINE_UNQUOTED(_PATH_NODEATTR, "$NODEATTR", [Path to nodeattr])
    AC_DEFINE(HAVE_GENDERS, 1, [Define if Genders is available])
    HAVE_GENDERS=1
fi
: ${HAVE_GENDERS=0}
AC_SUBST(HAVE_GENDERS)
AC_SUBST(NODEATTR)

if test "$withrms" = yes; then
    AC_PATH_PROG(RMSQUERY, rmsquery, [], /usr/bin:$PATH)
    if test -n "$RMSQUERY"; then
        AC_DEFINE_UNQUOTED(_PATH_RMSQUERY, "$RMSQUERY", [Path to rmsquery])
    else
        AC_MSG_ERROR([--with-rms specified but rmsquery program is not found])
    fi
fi
AC_SUBST(RMSQUERY)

AC_PATH_PROG(SDRGETOBJECTS, SDRGetObjects, "", /usr/lpp/ssp/bin:$PATH)
if test -n "$SDRGETOBJECTS"; then
    AC_DEFINE_UNQUOTED(_PATH_SDRGETOBJECTS, "$SDRGETOBJECTS", [Path SDRGetObjects])
    AC_DEFINE(HAVE_SDR, 1, [Define if SDR is available]) 
    HAVE_SDR=1
fi
: ${HAVE_SDR=0}
AC_SUBST(HAVE_SDR)
AC_SUBST(SDRGETOBJECTS)

AC_PATH_PROG(RCP, rcp, [], /usr/bin:$PATH)
if test -n "$RCP"; then
    AC_DEFINE_UNQUOTED(_PATH_RCP, "$RCP", [Path to rcp])
fi
AC_SUBST(RSH)

if test -z "$SDRGETOBJECTS" && test -z "$NODEATTR" && test -z "$MACHINES"; then
    AC_MSG_ERROR([did not find nodeattr, SDRGetObjects, or machines file])
fi

# Checks for libraries.
AC_CHECK_LIB([socket], [socket], LIBS="-lsocket -lnsl $LIBS",, [-lsocket -lnsl])
AC_CHECK_LIB([pthread], [pthread_create])
if test "$withelan" = yes || test "$withrms" = yes; then
    AC_CHECK_LIB([rmscall], [rms_prgcreate],, AC_MSG_ERROR([No librmscall!]))
fi
if test "$withelan" = yes; then
    AC_CHECK_LIB([elan3], [elan3_create],,AC_MSG_ERROR([No libelan3!]))
#    AC_CHECK_LIB([pam], [pam_start], LIBSPAM="-lpam -lpam_misc",
#	AC_MSG_ERROR([No libpam!]), [-lpam -lpam_misc])
fi
if test "$withrms" = yes; then
    AC_CHECK_LIB([rmsapi], [rms_numNodes], LIBS="-lrmsapi -lrms $LIBS",
	AC_MSG_ERROR([No librmsapi!]), [-lrmsapi -lrms])
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
  AC_HAVE_LIBRARY(dmalloc,
    [ AC_DEFINE_UNQUOTED(WITH_DMALLOC, 1,
        [Define if using the debug malloc library.]
      )
      LIBS="-ldmalloc $LIBS"
    ], [dmalloc=no]
  )
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
#else
#  define HAVE_MTSAFE_GETHOSTBYNAME 0
#  define HAVE_MAGIC_RSHELL_CLEANUP 0
#endif /* _AIX */]
)

AC_OUTPUT_COMMANDS(echo "creating dependencies"; make depend >/dev/null)
AC_OUTPUT([Makefile pdsh.spec pdcp.1 pdsh.1])

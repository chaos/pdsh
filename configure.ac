# Process this file with autoconf to produce a configure script.
# NOTE: We distribute ./configure and do not expect people building pdsh
# to have to regenerate from configure.ac.  However, if this is necessary,
# the ./mkconfig script is provided.  You may need autoconf 2.52 or greater
# (It does blow up with 2.13 on AIX).

AC_INIT(dsh.h)
AC_CONFIG_HEADER([conf.h])
AC_CONFIG_AUX_DIR([auxdir])

# determine project/version from META file
PROJECT="`perl -ne 'print,exit if s/^\s*NAME:\s*(\S*).*/\1/i' $srcdir/META`"
AC_DEFINE_UNQUOTED(PROJECT, "$PROJECT", [Define the project's name.])
AC_SUBST(PROJECT)
VERSION="`perl -ne 'print,exit if s/^\s*VERSION:\s*(\S*).*/\1/i' $srcdir/META`"
AC_DEFINE_UNQUOTED(VERSION, "$VERSION", [Define the project's version.])
AC_SUBST(VERSION)

# Enable features taht interact with external software
# --with-elan
AC_MSG_CHECKING(whether to support Elan interconnect)
AC_ARG_WITH(elan,
    AC_HELP_STRING([--with-elan], [Include QSW Elan support]),
    [ case "$withval" in
	yes)
	    withelan=yes
            AC_DEFINE(HAVE_ELAN3, 1, [define if building with Elan3 support])
            ELAN_PROGS=qshd
	    ;;
	no)
	    withelan=no
	    ;;
	*)
	    AC_MSG_RESULT(doh!)
	    AC_MSG_ERROR([bad value "$withval" for --with-elan])
	    ;;
    esac ]
)
AC_SUBST(ELAN_PROGS)
AC_MSG_RESULT(${withelan=no})

# --with-rms
AC_MSG_CHECKING(whether to support RMS)
AC_ARG_WITH(rms,
    AC_HELP_STRING([--with-rms], [Include RMS support]),
    [ case "$withval" in
	yes)
	    withrms=yes
            AC_DEFINE(HAVE_RMS_PMANAGER, 1, [define for RMS support])
	    ;;
	no)
	    withrms=no
	    ;;
	*)
	    AC_MSG_RESULT(doh!)
	    AC_MSG_ERROR([bad value "$withval" for --with-rms])
	    ;;
    esac ]
)
AC_MSG_RESULT(${withrms=no})

# --with-krb4
# or you can say --with-krb4=/usr/local/krb4
AC_MSG_CHECKING(whether to include Kerberos IV support)
AC_ARG_WITH(krb4,
    AC_HELP_STRING([--with-krb4], [Include Kerberos IV support]),
    [ case "$withval" in
	no)
	    withkrb4=no
	    ;;
	yes)
	    withkrb4=yes
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

# --with-genders
AC_MSG_CHECKING(whether to include Genders support)
AC_ARG_WITH(genders,
    AC_HELP_STRING([--with-genders], [Include Genders support]),
    [ case "$withval" in
	no)
	    withgenders=no
	    ;;
	yes)
	    withgenders=yes
            AC_DEFINE(HAVE_GENDERS, 1, [define for Genders support])
	    ;;
	*)
	    AC_MSG_RESULT(doh!)
	    AC_MSG_ERROR([bad value "$withval" for --with-genders])
	    ;;
    esac ]
)
AC_MSG_RESULT(${withgenders=no})

# Not yet configurable features
RANGE_OP='-'
AC_DEFINE_UNQUOTED(RANGE_OP, "$RANGE_OP", [Range operator])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_RANLIB
AC_PATH_PROG(SSH, ssh, [], /admin/bin:/usr/bin:/usr/local/bin:$PATH)
if test -n "$SSH"; then
    AC_DEFINE_UNQUOTED(_PATH_SSH, "$SSH", [Path to ssh])
fi
AC_PATH_PROG(NODEATTR, nodeattr, [], /usr/bin:/admin/scripts:$PATH)
if test -n "$NODEATTR"; then
    AC_DEFINE_UNQUOTED(_PATH_NODEATTR, "$NODEATTR", [Path to nodeattr])
fi
AC_PATH_PROG(RMSQUERY, rmsquery, [], /usr/bin:$PATH)
if test -n "$RMSQUERY"; then
    AC_DEFINE_UNQUOTED(_PATH_RMSQUERY, "$RMSQUERY", [Path to rmsquery])
fi
AC_PATH_PROG(SDRGETOBJECTS, SDRGetObjects, "", /usr/lpp/ssp/bin:$PATH)
if test -n "$SDRGETOBJECTS"; then
    AC_DEFINE_UNQUOTED(_PATH_SDRGETOBJECTS, "$SDRGETOBJECTS", [Path SDRGetObjects])
fi
AC_PATH_PROG(RCP, rcp, [], /usr/bin:$PATH)
if test -n "$RCP"; then
    AC_DEFINE_UNQUOTED(_PATH_RCP, "$RCP", [Path to rcp])
fi

# Checks for libraries.
AC_CHECK_LIB([socket], [socket], LIBS="-lsocket -lnsl $LIBS",, [-lsocket -lnsl])
AC_CHECK_LIB([pthread], [pthread_create])
if test "$withelan" = yes || test "$withrms" = yes; then
    AC_CHECK_LIB([rmscall], [rms_prgcreate],, AC_MSG_ERROR([No librmscall!]))
fi
if test "$withelan" = yes; then
    AC_CHECK_LIB([elan3], [elan3_create],,AC_MSG_ERROR([No libelan3!]))
fi
if test "$withrms" = yes; then
    AC_CHECK_LIB([rmsapi], [rms_numNodes], LIBS="-lrmsapi -lrms $LIBS",
	AC_MSG_ERROR([No librmsapi!]), [-lrmsapi -lrms])
fi
if test "$withkrb4" = yes; then
    AC_CHECK_LIB([krb], [krb_sendauth], LIBS="-lkrb -ldes $LIBS",
	AC_MSG_ERROR([No libkrb!]), [-lkrb -ldes])
fi

# Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([arpa/inet.h errno.h fcntl.h limits.h netdb.h netinet/in.h paths.h stdlib.h string.h strings.h sys/file.h sys/ioctl.h sys/param.h sys/socket.h sys/time.h syslog.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_UID_T

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_FUNC_FORK
AC_PROG_GCC_TRADITIONAL
AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_FUNC_SETPGRP
AC_TYPE_SIGNAL
AC_FUNC_STAT
AC_FUNC_STRERROR_R
AC_CHECK_FUNCS([alarm dup2 endpwent getcwd gethostbyaddr gethostbyname gethostname inet_ntoa isascii memset putenv select strchr strdup strerror strrchr strstr strtoul])

AC_OUTPUT_COMMANDS(echo "creating dependencies"; make depend >/dev/null)
AC_OUTPUT([Makefile pdsh.spec])

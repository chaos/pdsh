# Process this file with autoconf to produce a configure script.

AC_INIT(pdsh, 1.6, garlick@llnl.gov)
AC_CONFIG_SRCDIR([dsh.h])
AC_CONFIG_HEADER([conf.h])
AC_CONFIG_AUX_DIR([auxdir])
PROJECT=$PACKAGE_NAME
AC_DEFINE_UNQUOTED(PROJECT, "$PROJECT", [Project name])
AC_SUBST(PROJECT)
VERSION=$PACKAGE_VERSION
AC_DEFINE_UNQUOTED(VERSION, "$VERSION", [Project version])
AC_SUBST(VERSION)

# Enable features
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

AC_MSG_CHECKING(whether to include Kerberos IV support)
AC_ARG_WITH(krb4,
    AC_HELP_STRING([--with-krb4], [Include Kerberos IV support]),
    [ case "$withval" in
	no)
	    withkrb4=no
	    ;;
	yes)
	    withkrb4=yes
            AC_DEFINE(HAVE_KRB4, 1, [define for Kerberos IV support])
	    ;;
	*)
	    AC_MSG_RESULT(doh!)
	    AC_MSG_ERROR([bad value "$withval" for --with-krb4])
	    ;;
    esac ]
)
AC_MSG_RESULT(${withkrb4=no})

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
AC_CHECK_LIB([socket], [socket])
if test "$ac_cv_lib_socket_socket" = yes; then
    AC_CHECK_LIB([nsl], [inet_addr])
fi
AC_CHECK_LIB([pthread], [pthread_create])
if test "$withelan" = yes; then
    AC_CHECK_LIB([elan3], [elan3_create])
    AC_CHECK_LIB([rmscall], [rms_prgcreate])
fi
if test "$withrms" = yes; then
    AC_CHECK_LIB([rmsapi], [rms_numNodes])
    AC_CHECK_LIB([rms], [__builtin_new])
fi
if test "$withkrb4" = yes; then
    AC_CHECK_LIB([krb], [krb_sendauth])
    AC_CHECK_LIB([des], [des_string_to_key])
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
